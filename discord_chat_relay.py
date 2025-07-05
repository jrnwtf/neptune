import discord
from discord.ext import commands, tasks
import json
import os
import asyncio
import logging
from datetime import datetime, timedelta, timezone
from pathlib import Path
import time
from typing import Dict, Set
import traceback

# Configure logging
logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s - %(levelname)s - %(message)s',
    handlers=[
        logging.FileHandler('discord_bot.log'),
        logging.StreamHandler()
    ]
)
logger = logging.getLogger(__name__)

class ChatRelayBot(commands.Bot):
    def __init__(self):
        intents = discord.Intents.default()
        intents.message_content = True
        
        super().__init__(
            command_prefix='!',
            intents=intents,
            help_command=None
        )
        
        # Load configuration
        self.config = self.load_config()
        
        # Configuration
        self.DISCORD_TOKEN = self.config.get('discord_token', 'YOUR_BOT_TOKEN_HERE')
        self.CHANNEL_ID = self.config.get('channel_id', 123456789012345678)
        self.AMALGAM_PATH = os.path.expandvars(r'%APPDATA%\Amalgam')
        
        # Settings
        settings = self.config.get('settings', {})
        self.CHECK_INTERVAL = settings.get('check_interval_seconds', 2)
        self.MAX_MESSAGE_CACHE = settings.get('max_message_cache', 1000)
        self.DEBUG_LOGGING = settings.get('enable_debug_logging', False)
        
        # Formatting
        formatting = self.config.get('formatting', {})
        self.team_colors = {
            k: int(v, 16) if isinstance(v, str) else v 
            for k, v in formatting.get('team_colors', {
                'RED': 0xff4444, 'BLU': 0x4444ff, 'SPEC': 0x888888, 'UNK': 0xcccccc
            }).items()
        }
        self.team_emojis = formatting.get('team_emojis', {
            'RED': '🔴', 'BLU': '🔵', 'SPEC': '👁️', 'UNK': '❓'
        })
        
        # Set debug logging if enabled
        if self.DEBUG_LOGGING:
            logging.getLogger().setLevel(logging.DEBUG)
        
        # State tracking
        self.processed_files: Dict[str, int] = {}  # filename -> last_position
        self.processed_messages: Set[str] = set()  # message hashes to prevent duplicates
        self.target_channel = None
        self.monitor_task = None
        
        # File monitoring
        self.last_check = time.time()
        
    def load_config(self):
        """Load configuration from config.json"""
        if not os.path.exists('config.json'):
            logger.warning("config.json not found, using default values")
            return {}
            
        # Try multiple encodings
        encodings = ['utf-8', 'utf-8-sig', 'latin1', 'cp1252']
        
        for encoding in encodings:
            try:
                with open('config.json', 'r', encoding=encoding) as f:
                    content = f.read()
                    # Remove any potential BOM or invalid characters
                    content = content.strip()
                    if content.startswith('\ufeff'):
                        content = content[1:]
                    return json.loads(content)
            except (UnicodeDecodeError, json.JSONDecodeError) as e:
                logger.debug(f"Failed to load config with encoding {encoding}: {e}")
                continue
            except Exception as e:
                logger.debug(f"Unexpected error with encoding {encoding}: {e}")
                continue
        
        logger.error("Could not load config.json with any encoding, using defaults")
        return {}
        
    async def on_ready(self):
        logger.info(f'Bot connected as {self.user}')
        
        # Get target channel
        self.target_channel = self.get_channel(self.CHANNEL_ID)
        if not self.target_channel:
            logger.error(f"Could not find channel with ID {self.CHANNEL_ID}")
            return
            
        logger.info(f'Target channel: #{self.target_channel.name}')
        
        # Start monitoring task
        await self.start_monitoring()
            
        # Send startup message
        try:
            embed = discord.Embed(
                title="🤖 TF2 Chat Relay Bot",
                description="Now monitoring Amalgam chat logs...",
                color=0x00ff00,
                timestamp=datetime.now(timezone.utc)
            )
            await self.target_channel.send(embed=embed)
        except Exception as e:
            logger.error(f"Failed to send startup message: {e}")

    async def start_monitoring(self):
        """Start the monitoring task if it's not already running."""
        if self.monitor_task and self.monitor_task.is_running():
            return

        @tasks.loop(seconds=self.CHECK_INTERVAL)
        async def monitor_task():
            await self.monitor_chat_logs()

        self.monitor_task = monitor_task
        self.monitor_task.start()
    
    async def monitor_chat_logs(self):
        try:
            # Update last check timestamp
            self.last_check = time.time()
            
            if not self.target_channel:
                return
                
            # Check if Amalgam directory exists
            if not os.path.exists(self.AMALGAM_PATH):
                return
                
            # Find all chat relay log files
            log_files = []
            for file in os.listdir(self.AMALGAM_PATH):
                if file.startswith('chat_relay_') and file.endswith('.log'):
                    log_files.append(os.path.join(self.AMALGAM_PATH, file))
            
            # Process each log file
            for log_file in log_files:
                await self.process_log_file(log_file)
                
        except Exception as e:
            logger.error(f"Error in monitor_chat_logs: {e}")
            logger.error(traceback.format_exc())

    async def process_log_file(self, log_file: str):
        try:
            filename = os.path.basename(log_file)
            
            # Get file size and last position
            if not os.path.exists(log_file):
                return
                
            current_size = os.path.getsize(log_file)
            last_position = self.processed_files.get(filename, 0)
            
            # If file hasn't grown, nothing to do
            if current_size <= last_position:
                return
                
            # Read new content
            with open(log_file, 'r', encoding='utf-8', errors='ignore') as f:
                f.seek(last_position)
                new_content = f.read()
                
            # Process new lines
            if new_content.strip():
                lines = new_content.strip().split('\n')
                for line in lines:
                    if line.strip():
                        await self.process_chat_message(line.strip())
                        
            # Update position
            self.processed_files[filename] = current_size
            
        except Exception as e:
            logger.error(f"Error processing log file {log_file}: {e}")

    async def process_chat_message(self, json_line: str):
        try:
            # Parse JSON
            data = json.loads(json_line)
            
            # Extract data
            timestamp = data.get('timestamp', '')
            server = data.get('server', 'Unknown Server')
            player = data.get('player', 'Unknown Player')
            team = data.get('team', 'UNK')
            message = data.get('message', '')
            is_team_chat = data.get('teamchat', False)
            
            # Create message hash to prevent duplicates
            message_hash = f"{timestamp}:{player}:{message}"
            if message_hash in self.processed_messages:
                return
                
            self.processed_messages.add(message_hash)
            
            # Clean up old hashes (keep only configured amount)
            if len(self.processed_messages) > self.MAX_MESSAGE_CACHE:
                cleanup_count = min(100, len(self.processed_messages) // 10)
                old_hashes = list(self.processed_messages)[:cleanup_count]
                for old_hash in old_hashes:
                    self.processed_messages.discard(old_hash)
            
            # Format and send Discord message
            await self.send_chat_to_discord(timestamp, server, player, team, message, is_team_chat)
            
        except json.JSONDecodeError as e:
            logger.warning(f"Failed to parse JSON: {json_line[:100]}... Error: {e}")
        except Exception as e:
            logger.error(f"Error processing chat message: {e}")

    async def send_chat_to_discord(self, timestamp: str, server: str, player: str, team: str, message: str, is_team_chat: bool):
        try:
            # Use configured colors and emojis
            color = self.team_colors.get(team, 0xcccccc)
            team_emoji = self.team_emojis.get(team, '❓')
            
            # Chat type indicator
            chat_type = "💬 **TEAM**" if is_team_chat else "🌐 **ALL**"
            
            # Create embed
            embed = discord.Embed(
                color=color,
                timestamp=datetime.now(timezone.utc)
            )
            
            # Format server name (truncate if too long)
            server_display = server[:30] + "..." if len(server) > 30 else server
            
            # Set author (player info)
            embed.set_author(
                name=f"{team_emoji} {player} [{team}]",
                icon_url=f"https://via.placeholder.com/32/{team.lower()}/ffffff?text={team[0]}"
            )
            
            # Message content
            embed.description = f"{chat_type}\n```{message}```"
            
            # Footer with server and timestamp info
            embed.set_footer(
                text=f"📡 {server_display} • {timestamp}",
                icon_url="https://via.placeholder.com/16/ffaa00/ffffff?text=TF2"
            )
            
            # Send to Discord
            await self.target_channel.send(embed=embed)
            
            logger.info(f"Sent message: [{team}] {player}: {message[:50]}...")
            
        except Exception as e:
            logger.error(f"Failed to send message to Discord: {e}")
            # Fallback to simple text message
            try:
                simple_msg = f"`[{team}] {player}:` {message}"
                if is_team_chat:
                    simple_msg = f"**TEAM** {simple_msg}"
                await self.target_channel.send(simple_msg[:2000])  # Discord limit
            except:
                logger.error("Failed to send even simple message")

    @commands.command(name='status')
    async def status_command(self, ctx):
        """Check bot status and stats"""
        try:
            # Get stats
            files_monitored = len(self.processed_files)
            total_messages = len(self.processed_messages)
            
            # Check if Amalgam directory exists
            amalgam_exists = os.path.exists(self.AMALGAM_PATH)
            
            # Find current log files
            current_logs = 0
            if amalgam_exists:
                for file in os.listdir(self.AMALGAM_PATH):
                    if file.startswith('chat_relay_') and file.endswith('.log'):
                        current_logs += 1
            
            embed = discord.Embed(
                title="📊 Chat Relay Bot Status",
                color=0x00ff00 if amalgam_exists else 0xff0000,
                timestamp=datetime.now(timezone.utc)
            )
            
            embed.add_field(
                name="🗂️ Monitoring",
                value=f"Files tracked: {files_monitored}\nCurrent logs: {current_logs}",
                inline=True
            )
            
            embed.add_field(
                name="💬 Messages",
                value=f"Processed: {total_messages}\nLast check: {datetime.fromtimestamp(self.last_check).strftime('%H:%M:%S')}",
                inline=True
            )
            
            embed.add_field(
                name="📁 Amalgam Path",
                value=f"{'✅ Found' if amalgam_exists else '❌ Missing'}\n`{self.AMALGAM_PATH}`",
                inline=False
            )
            
            await ctx.send(embed=embed)
            
        except Exception as e:
            await ctx.send(f"❌ Error getting status: {e}")

    @commands.command(name='logs')
    async def logs_command(self, ctx, lines: int = 10):
        """Show recent log entries"""
        try:
            if lines > 50:
                lines = 50
                
            log_content = []
            try:
                with open('discord_bot.log', 'r', encoding='utf-8', errors='ignore') as f:
                    all_lines = f.readlines()
                    recent_lines = all_lines[-lines:]
                    log_content = [line.strip() for line in recent_lines]
            except:
                log_content = ["No log file found"]
            
            if log_content:
                content = '\n'.join(log_content)
                if len(content) > 1900:  # Discord limit with some buffer
                    content = content[-1900:]
                    content = "...\n" + content
                
                embed = discord.Embed(
                    title=f"📝 Recent Log Entries ({lines} lines)",
                    description=f"```\n{content}\n```",
                    color=0xffaa00,
                    timestamp=datetime.now(timezone.utc)
                )
                await ctx.send(embed=embed)
            else:
                await ctx.send("No log entries found.")
                
        except Exception as e:
            await ctx.send(f"❌ Error reading logs: {e}")

    @commands.command(name='reload')
    @commands.has_permissions(administrator=True)
    async def reload_command(self, ctx):
        """Reload the monitoring task"""
        try:
            await self.start_monitoring()
            await ctx.send("✅ Monitoring task running!")
        except Exception as e:
            await ctx.send(f"❌ Error reloading: {e}")

    async def on_command_error(self, ctx, error):
        if isinstance(error, commands.MissingPermissions):
            await ctx.send("❌ You don't have permission to use this command.")
        elif isinstance(error, commands.CommandNotFound):
            pass  # Ignore unknown commands
        else:
            logger.error(f"Command error: {error}")
            await ctx.send(f"❌ An error occurred: {error}")
    
    async def close(self):
        """Clean up when bot shuts down"""
        try:
            if hasattr(self, 'monitor_task') and self.monitor_task:
                self.monitor_task.cancel()
                logger.info("Monitoring task stopped")
        except Exception as e:
            logger.error(f"Error stopping monitoring task: {e}")
        
        await super().close()

def main():
    """Main function to run the bot"""
    bot = ChatRelayBot()
    
    # Check configuration
    if bot.DISCORD_TOKEN == "YOUR_BOT_TOKEN_HERE":
        logger.error("Please configure your Discord bot token!")
        logger.error("Edit config.json and set 'discord_token' to your bot token")
        logger.error("Or edit discord_chat_relay.py directly")
        return
        
    if bot.CHANNEL_ID == 123456789012345678:
        logger.error("Please configure your Discord channel ID!")
        logger.error("Edit config.json and set 'channel_id' to your channel ID")
        logger.error("Or edit discord_chat_relay.py directly")
        return
    
    logger.info("Starting TF2 Chat Relay Discord Bot...")
    logger.info(f"Configuration loaded: {len(bot.config)} settings")
    logger.info(f"Monitoring directory: {bot.AMALGAM_PATH}")
    logger.info(f"Target channel ID: {bot.CHANNEL_ID}")
    logger.info(f"Check interval: {bot.CHECK_INTERVAL} seconds")
    
    try:
        bot.run(bot.DISCORD_TOKEN)
    except Exception as e:
        logger.error(f"Failed to start bot: {e}")

if __name__ == "__main__":
    main() 