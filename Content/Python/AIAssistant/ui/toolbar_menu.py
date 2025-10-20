"""
UE5 Editor Toolbar Menu Extension for AI Assistant
Adds a toolbar dropdown with easy access to troubleshooting tools
"""

def register_toolbar_menu():
    """Register AI Assistant menu in UE5 editor toolbar"""
    try:
        import unreal
    except ImportError:
        print("⚠️ Not in UE5 environment - skipping toolbar registration")
        return
    
    try:
        # Get the menu system
        menus = unreal.ToolMenus.get()
        
        # Find or create the main menu bar
        main_menu = menus.find_menu("LevelEditor.MainMenu")
        if not main_menu:
            print("⚠️ Could not find main menu bar")
            return
        
        # Create AI Assistant menu section
        ai_menu = main_menu.add_sub_menu(
            owner=main_menu.menu_name,
            section_name="",
            name="AIAssistant",
            label="AI Assistant"
        )
        
        # Add Launch Troubleshooter menu item
        troubleshooter_entry = unreal.ToolMenuEntry(
            name="LaunchTroubleshooter",
            type=unreal.MultiBlockType.MENU_ENTRY
        )
        troubleshooter_entry.set_label(unreal.Text("🔧 Launch Troubleshooter"))
        troubleshooter_entry.set_tool_tip(unreal.Text("Import troubleshooter tools and show available commands"))
        troubleshooter_entry.set_string_command(
            type=unreal.ToolMenuStringCommandType.PYTHON,
            custom_type=unreal.Name(""),
            string="import AIAssistant.troubleshoot.troubleshooter as ts; ts.help(); print('\\n✅ Troubleshooter loaded! Use ts.reconnect(), ts.status(), ts.info(), ts.dashboard()')"
        )
        ai_menu.add_menu_entry("AIAssistantCommands", troubleshooter_entry)
        
        # Add Restart Assistant menu item
        restart_entry = unreal.ToolMenuEntry(
            name="RestartAssistant",
            type=unreal.MultiBlockType.MENU_ENTRY
        )
        restart_entry.set_label(unreal.Text("🔄 Restart Assistant"))
        restart_entry.set_tool_tip(unreal.Text("Restart the AI Assistant connection (fixes most connection issues)"))
        restart_entry.set_string_command(
            type=unreal.ToolMenuStringCommandType.PYTHON,
            custom_type=unreal.Name(""),
            string="from AIAssistant.system.auto_update import force_restart_assistant; force_restart_assistant(); print('\\n✅ Assistant restarted! Connection re-established.')"
        )
        ai_menu.add_menu_entry("AIAssistantCommands", restart_entry)
        
        # Add Test Connection menu item
        test_entry = unreal.ToolMenuEntry(
            name="TestConnection",
            type=unreal.MultiBlockType.MENU_ENTRY
        )
        test_entry.set_label(unreal.Text("🔍 Test Connection"))
        test_entry.set_tool_tip(unreal.Text("Run diagnostics to check connection status"))
        test_entry.set_string_command(
            type=unreal.ToolMenuStringCommandType.PYTHON,
            custom_type=unreal.Name(""),
            string="import AIAssistant.troubleshoot.troubleshooter as ts; ts.status(); ts.info(); print('\\n💡 Use ts.reconnect() to fix connection issues')"
        )
        ai_menu.add_menu_entry("AIAssistantCommands", test_entry)
        
        # Add Open Dashboard menu item
        dashboard_entry = unreal.ToolMenuEntry(
            name="OpenDashboard",
            type=unreal.MultiBlockType.MENU_ENTRY
        )
        dashboard_entry.set_label(unreal.Text("🌐 Open Dashboard"))
        dashboard_entry.set_tool_tip(unreal.Text("Open the web dashboard in browser"))
        dashboard_entry.set_string_command(
            type=unreal.ToolMenuStringCommandType.PYTHON,
            custom_type=unreal.Name(""),
            string="from AIAssistant.core.config import get_config; import webbrowser; url = get_config().api_url; webbrowser.open(f'{url}/dashboard'); print(f'\\n✅ Dashboard opened: {url}/dashboard')"
        )
        ai_menu.add_menu_entry("AIAssistantCommands", dashboard_entry)
        
        # Add separator for server management
        separator_server = unreal.ToolMenuEntry(
            name="SeparatorServer",
            type=unreal.MultiBlockType.SEPARATOR
        )
        ai_menu.add_menu_entry("AIAssistantServer", separator_server)
        
        # Add Update to Current Server menu item
        update_server_entry = unreal.ToolMenuEntry(
            name="UpdateToCurrentServer",
            type=unreal.MultiBlockType.MENU_ENTRY
        )
        update_server_entry.set_label(unreal.Text("🔄 Update to Production Server"))
        update_server_entry.set_tool_tip(unreal.Text("Fix connection to production server (ue5-assistant-noahbutcher97.replit.app)"))
        update_server_entry.set_string_command(
            type=unreal.ToolMenuStringCommandType.PYTHON,
            custom_type=unreal.Name(""),
            string="from AIAssistant.core.config import get_config; config = get_config(); config.SERVER_PRESETS['production'] = 'https://ue5-assistant-noahbutcher97.replit.app'; config.set('api_base_url', 'https://ue5-assistant-noahbutcher97.replit.app'); config.set('active_server', 'production'); config.save(); print('\\n' + '='*70); print('✅ Updated to production server!'); print('   URL: https://ue5-assistant-noahbutcher97.replit.app'); print('='*70); print('\\n🔄 Restarting assistant...'); from AIAssistant.system.auto_update import force_restart_assistant; force_restart_assistant()"
        )
        ai_menu.add_menu_entry("AIAssistantServer", update_server_entry)
        
        # Add Show Current Server menu item
        show_server_entry = unreal.ToolMenuEntry(
            name="ShowCurrentServer",
            type=unreal.MultiBlockType.MENU_ENTRY
        )
        show_server_entry.set_label(unreal.Text("📡 Show Current Server"))
        show_server_entry.set_tool_tip(unreal.Text("Display which server you're currently connected to"))
        show_server_entry.set_string_command(
            type=unreal.ToolMenuStringCommandType.PYTHON,
            custom_type=unreal.Name(""),
            string="from AIAssistant.core.config import get_config; config = get_config(); print('\\n' + '='*70); print('📡 CURRENT SERVER CONNECTION'); print('='*70); print(f'Active Server: {config.get(\"active_server\", \"unknown\")}'); print(f'Backend URL: {config.api_url}'); print('='*70); print('\\n💡 Use \"Update to Current Server\" if dashboard shows disconnected')"
        )
        ai_menu.add_menu_entry("AIAssistantServer", show_server_entry)
        
        # Add Switch to Localhost menu item
        localhost_entry = unreal.ToolMenuEntry(
            name="SwitchToLocalhost",
            type=unreal.MultiBlockType.MENU_ENTRY
        )
        localhost_entry.set_label(unreal.Text("🏠 Switch to Localhost"))
        localhost_entry.set_tool_tip(unreal.Text("Connect to local development server (http://localhost:5000)"))
        localhost_entry.set_string_command(
            type=unreal.ToolMenuStringCommandType.PYTHON,
            custom_type=unreal.Name(""),
            string="from AIAssistant.core.config import get_config; config = get_config(); config.switch_server('localhost'); config.save(); print('\\n✅ Switched to localhost server'); print('\\n🔄 Restarting assistant...'); from AIAssistant.system.auto_update import force_restart_assistant; force_restart_assistant()"
        )
        ai_menu.add_menu_entry("AIAssistantServer", localhost_entry)
        
        # Add separator
        separator1 = unreal.ToolMenuEntry(
            name="Separator1",
            type=unreal.MultiBlockType.SEPARATOR
        )
        ai_menu.add_menu_entry("AIAssistantMaintenance", separator1)
        
        # Add Clean Duplicate Files menu item
        cleanup_entry = unreal.ToolMenuEntry(
            name="CleanupLegacy",
            type=unreal.MultiBlockType.MENU_ENTRY
        )
        cleanup_entry.set_label(unreal.Text("🧹 Clean Duplicate Files"))
        cleanup_entry.set_tool_tip(unreal.Text("Remove legacy duplicate files from old installations"))
        cleanup_entry.set_string_command(
            type=unreal.ToolMenuStringCommandType.PYTHON,
            custom_type=unreal.Name(""),
            string="from AIAssistant.system.cleanup_legacy import cleanup_legacy_files; cleanup_legacy_files()"
        )
        ai_menu.add_menu_entry("AIAssistantMaintenance", cleanup_entry)
        
        # Add Clean Cache menu item
        cache_entry = unreal.ToolMenuEntry(
            name="CleanCache",
            type=unreal.MultiBlockType.MENU_ENTRY
        )
        cache_entry.set_label(unreal.Text("🗑️ Clean __pycache__"))
        cache_entry.set_tool_tip(unreal.Text("Remove all Python cache directories"))
        cache_entry.set_string_command(
            type=unreal.ToolMenuStringCommandType.PYTHON,
            custom_type=unreal.Name(""),
            string="from AIAssistant.system.cleanup_legacy import cleanup_pycache_recursive; cleanup_pycache_recursive()"
        )
        ai_menu.add_menu_entry("AIAssistantMaintenance", cache_entry)
        
        # Add Verify Installation menu item
        verify_entry = unreal.ToolMenuEntry(
            name="VerifyInstallation",
            type=unreal.MultiBlockType.MENU_ENTRY
        )
        verify_entry.set_label(unreal.Text("✅ Verify Installation"))
        verify_entry.set_tool_tip(unreal.Text("Check directory structure and file consistency"))
        verify_entry.set_string_command(
            type=unreal.ToolMenuStringCommandType.PYTHON,
            custom_type=unreal.Name(""),
            string="from AIAssistant.system.cleanup_legacy import get_legacy_files; from pathlib import Path; import unreal; ai_root = Path(unreal.Paths.project_dir()) / 'Content' / 'Python' / 'AIAssistant'; legacy = [f for f in get_legacy_files() if (ai_root / f).exists()]; print('\\n' + '='*60); print('📊 Installation Verification'); print('='*60); print(f'Legacy duplicates found: {len(legacy)}'); [print(f'  ⚠️  {f}') for f in legacy[:5]]; print('\\n✅ Run \"Clean Duplicate Files\" to remove them' if legacy else '\\n✅ Installation is clean!'); print('='*60)"
        )
        ai_menu.add_menu_entry("AIAssistantMaintenance", verify_entry)
        
        # Add separator
        separator2 = unreal.ToolMenuEntry(
            name="Separator2",
            type=unreal.MultiBlockType.SEPARATOR
        )
        ai_menu.add_menu_entry("AIAssistantUpdates", separator2)
        
        # Add Force Update menu item
        update_entry = unreal.ToolMenuEntry(
            name="ForceUpdate",
            type=unreal.MultiBlockType.MENU_ENTRY
        )
        update_entry.set_label(unreal.Text("⬇️ Force Update"))
        update_entry.set_tool_tip(unreal.Text("Download and install latest version from backend"))
        update_entry.set_string_command(
            type=unreal.ToolMenuStringCommandType.PYTHON,
            custom_type=unreal.Name(""),
            string="from AIAssistant.system.auto_update import check_and_update; check_and_update(); print('\\n✅ Update complete! Restarting assistant...'); from AIAssistant.system.auto_update import force_restart_assistant; force_restart_assistant()"
        )
        ai_menu.add_menu_entry("AIAssistantUpdates", update_entry)
        
        # Refresh menus to show changes
        menus.refresh_all_widgets()
        
        unreal.log("✅ AI Assistant toolbar menu registered")
        print("✅ AI Assistant menu added to editor toolbar")
        
    except Exception as e:
        print(f"❌ Failed to register toolbar menu: {e}")
        import traceback
        traceback.print_exc()


def unregister_toolbar_menu():
    """Remove AI Assistant menu from toolbar (for cleanup)"""
    try:
        import unreal
        menus = unreal.ToolMenus.get()
        main_menu = menus.find_menu("LevelEditor.MainMenu")
        if main_menu:
            main_menu.remove_sub_menu("AIAssistant")
            menus.refresh_all_widgets()
            print("✅ AI Assistant menu removed from toolbar")
    except Exception as e:
        print(f"❌ Failed to unregister toolbar menu: {e}")
