/*
 * Darke Positional Audio Receiver for Teamspeak 3
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <assert.h>
#include <map>
#include "cpprest/http_client.h"
#include "cpprest/json.h"
#include "cpprest/uri.h"
#include "cpprest/uri_builder.h"
#include "cpprest\producerconsumerstream.h"
#include "teamspeak/public_errors.h"
#include "teamspeak/public_errors_rare.h"
#include "teamspeak/public_definitions.h"
#include "teamspeak/public_rare_definitions.h"
#include "teamspeak/clientlib_publicdefinitions.h"
#include "ts3_functions.h"
#include "timercpp.h"
#include "plugin.hpp"

using namespace utility;                    // Common utilities like string conversions
using namespace web;                        // Common features like URIs.
using namespace web::http;                  // Common HTTP functionality
using namespace web::http::client;          // HTTP client features
using namespace concurrency::streams;       // Asynchronous streams
using namespace web::json;                  // JSON Lib
using namespace std;

static struct TS3Functions ts3Functions;

#ifdef _WIN32
#define _strcpy(dest, destSize, src) strcpy_s(dest, destSize, src)
#define snprintf sprintf_s
#else
#define _strcpy(dest, destSize, src) { strncpy(dest, src, destSize-1); (dest)[destSize-1] = '\0'; }
#endif

#define PLUGIN_API_VERSION 23

#define PATH_BUFSIZE 512
#define COMMAND_BUFSIZE 128
#define INFODATA_BUFSIZE 128
#define SERVERINFO_BUFSIZE 256
#define CHANNELINFO_BUFSIZE 512
#define RETURNCODE_BUFSIZE 128

static char* pluginID = NULL;

bool globallyEnabled = true;

float RolloffOffset = 20.0f;
float RolloffCutoff = 60.0f;
float RolloffAttenuationCoefficient = 0.2f;
bool CanHearUnregistered = true;

int UpdatesPerSecond = 15;

std::string ServerHost = "wolfz.uk";
std::string ServerPort = "9000";

bool ChannelHasConfig = false;

Timer t;

/********************************** DPAR plugin functions *********************************/
#pragma region DPARFunctions

void dpar_setIntervalForTimer(uint64 serverConnectionHandlerID) {
	ts3Functions.logMessage("Restarting Timer (dpar_setIntervalForTimer)", LogLevel_DEBUG, "DPAR", serverConnectionHandlerID);
	t.setInterval(&dpar_update3Dposition, serverConnectionHandlerID, 1000 / UpdatesPerSecond);
}

void dpar_onPluginEnable(uint64 serverConnectionHandlerID) {
	globallyEnabled = true;
	ts3Functions.logMessage("Positional audio enabled", LogLevel_INFO, "DPAR", serverConnectionHandlerID);
}

void dpar_onPluginDisable(uint64 serverConnectionHandlerID) {
	globallyEnabled = false;
	ts3Functions.logMessage("Positional audio disabled", LogLevel_INFO, "DPAR", serverConnectionHandlerID);
}

void dpar_updateFromRemoteConfiguration(uint64 serverConnectionHandlerID) {
	ts3Functions.logMessage("Attempting to get attenuation config from remote", LogLevel_INFO, "DPAR", serverConnectionHandlerID);

	std::string candidateUri = "http://" + std::string(ServerHost) + std::string(":") + std::string(ServerPort);
	utility::string_t concatStr = utility::conversions::to_string_t(candidateUri);
	web::http::client::http_client client(concatStr);
	uri_builder builder(U("/config"));

	try {
		http_response response = client.request(methods::GET, builder.to_string()).get();

		//Parse network response
		json::object jsonVal = response.extract_json().get().as_object();

		RolloffCutoff = jsonVal[L"cutoffDistance"].as_double();
		RolloffAttenuationCoefficient = 1 / jsonVal[L"attenuationCoefficient"].as_double();
		RolloffOffset = jsonVal[L"safeZoneSize"].as_double();
		CanHearUnregistered = jsonVal[L"unregisteredCanBroadcast"].as_bool();

		ts3Functions.logMessage("Successfully updated attenuation config from remote", LogLevel_INFO, "DPAR", serverConnectionHandlerID);
	}
	catch (const std::exception& e) {
		ts3Functions.logMessage("Failed to load attenuation config from remote", LogLevel_ERROR, "DPAR", serverConnectionHandlerID);
	}
}

anyID dpar_getMyClientID(uint64 serverConnectionHandlerID) {
	anyID id = NULL;
	ts3Functions.getClientID(serverConnectionHandlerID, &id);
	if (id == NULL) {
		ts3Functions.logMessage("Failed to get own client ID", LogLevel_ERROR, "DPAR", serverConnectionHandlerID);
		return 0;
	}
	return id;
}

uint64 dpar_getMyCurrentChannel(uint64 serverConnectionHandlerID) {
	anyID id = dpar_getMyClientID(serverConnectionHandlerID);

	uint64 channelID = NULL;
	ts3Functions.getChannelOfClient(serverConnectionHandlerID, id, &channelID);
	if (channelID == NULL) {
		ts3Functions.logMessage("Failed to get own channel ID", LogLevel_WARNING, "DPAR", serverConnectionHandlerID);
		return 0;
	}

	return channelID;
}

string dpar_getMyClientUID(uint64 serverConnectionHandlerID) {

	anyID id = dpar_getMyClientID(serverConnectionHandlerID);

	char* meclientUID = NULL;
	ts3Functions.getClientVariableAsString(serverConnectionHandlerID, id, CLIENT_UNIQUE_IDENTIFIER, &meclientUID);
	if (meclientUID == NULL) {
		ts3Functions.logMessage("Failed to get own identity ID", LogLevel_ERROR, "DPAR", serverConnectionHandlerID);
		return "";
	}
	std::string meclientUIDstr(meclientUID);
	return meclientUIDstr;
}


void dpar_updateCurrentReportingServerConfig(std::string serverAddress, std::string serverPort) {
	ServerHost = serverAddress;
	ServerPort = serverPort;
}

void dpar_updateConfigFromChannelDescription(uint64 serverConnectionHandlerID, uint64 channelID) {

	ts3Functions.logMessage("Attempting to get remote host's config from channel", LogLevel_INFO, "DPAR", serverConnectionHandlerID);

	char* channelDesc;
	ts3Functions.getChannelVariableAsString(serverConnectionHandlerID, channelID, CHANNEL_DESCRIPTION, &channelDesc);

	std::string channelDescStr(channelDesc);

	int startPos = channelDescStr.find_first_of("|");
	if (!(startPos == -1l)) {
		int midPos = channelDescStr.find_first_of("|", startPos + 1);

		if (!(midPos == -1l)) {
			std::string serverAddress = channelDescStr.substr(startPos + 1, midPos - (startPos + 1));

			int endPos = channelDescStr.find_first_of("|", midPos + 1);

			if (!(endPos == -1l)) {
				ts3Functions.logMessage("Host and port config found", LogLevel_INFO, "DPAR", serverConnectionHandlerID);

				std::string serverPort = channelDescStr.substr(midPos + 1, endPos - (midPos + 1));

				printf("PLUGIN: Server Setting: %s %s\n", serverAddress, serverPort);
				dpar_updateCurrentReportingServerConfig(serverAddress, serverPort);
				ChannelHasConfig = true;
			}
			else {
				ts3Functions.logMessage("Host only config found", LogLevel_INFO, "DPAR", serverConnectionHandlerID);

				std::string serverPort = "9000";

				printf("PLUGIN: Server Setting: %s %s\n", serverAddress, serverPort);
				dpar_updateCurrentReportingServerConfig(serverAddress, serverPort);
				ChannelHasConfig = true;
			}
		}
		else {
			ts3Functions.logMessage("No valid config found (Single '|')", LogLevel_INFO, "DPAR", serverConnectionHandlerID);
			ChannelHasConfig = false;
		}
	}
	else {
		ts3Functions.logMessage("No valid config found", LogLevel_INFO, "DPAR", serverConnectionHandlerID);
		ChannelHasConfig = false;
	}
}

void dpar_update3Dposition(uint64 serverConnectionHandlerID) {
	printf("DPAR: Update position\n");
	bool enabled = globallyEnabled;

	uint64 currentChannelID = dpar_getMyCurrentChannel(serverConnectionHandlerID);

	string localClientUID = dpar_getMyClientUID(serverConnectionHandlerID);

	anyID* clientidlist = NULL;
	ts3Functions.getChannelClientList(serverConnectionHandlerID, currentChannelID, &clientidlist);
	if (clientidlist == NULL) {
		return;
	}

	TS3_VECTOR center;
	center.x = 0.0f;
	center.y = 0.0f;
	center.z = 0.0f;

	//TS3_VECTOR forward;
	//forward.x = 0.0f;
	//forward.y = 0.0f;
	//forward.z = 1.0f;

	TS3_VECTOR up;
	up.x = 0.0f;
	up.y = 1.0f;
	up.z = 0.0f;

	if (!enabled) {
		//While clientidlist[i] not null
		for (int i = 0; clientidlist[i]; ++i) {
			char* clientUID = NULL;
			ts3Functions.getClientVariableAsString(serverConnectionHandlerID, clientidlist[i], CLIENT_UNIQUE_IDENTIFIER, &clientUID);
			if (clientUID == NULL) {
				return;
			}

			std::string clientstr(clientUID);
			string_t clientid = conversions::to_string_t(clientstr);

			TS3_VECTOR position;

			position.x = 0.0f;
			position.y = 0.0f;
			position.z = 0.0f;
			ts3Functions.channelset3DAttributes(serverConnectionHandlerID, clientidlist[i], &position);
		}
		printf("POS Audio Disabled\n");
		return;
	}

	// Create http_client to send the request.
	const std::string candidateUri = "http://" + ServerHost + ":" + ServerPort + "";

	utility::string_t concatStr = utility::conversions::to_string_t(candidateUri);

	web::http::client::http_client client(concatStr);

	try {
		// Build request URI and start the request.
		uri_builder builder(U("/request"));
		builder.append_query(U("id"), conversions::to_string_t(localClientUID)); //May turn the pointer to a string not the ID.... std::to_string(*id)
		http_response response = client.request(methods::GET, builder.to_string()).get();

		//Parse network response
		json::object jsonVal = response.extract_json().get().as_object();

		//While clientidlist[i] not null
		for (int i = 0; clientidlist[i]; ++i) {

			char* clientUID = NULL;
			ts3Functions.getClientVariableAsString(serverConnectionHandlerID, clientidlist[i], CLIENT_UNIQUE_IDENTIFIER, &clientUID);
			if (clientUID == NULL) {
				return;
			}
			std::string clientstr(clientUID);
			string_t clientid = conversions::to_string_t(clientstr);

			if (jsonVal.size == 0) {

				// If it's ourselves
				if (localClientUID == clientstr) {
					continue;
				}

				// If its someone else
				TS3_VECTOR position;
				position.x = 0.0f;
				position.y = 0.0f;
				position.z = 0.0f;

				if (!CanHearUnregistered) {
					position.y = -1024.0f;
				}

				ts3Functions.channelset3DAttributes(serverConnectionHandlerID, clientidlist[i], &position);

				continue;
			}


			//Set cross matched users 3D positions
			if (jsonVal[conversions::to_string_t(std::to_string(clientidlist[i]))] != NULL) {

				// We have data for a user and they're not the local user
				if (jsonVal[clientid].is_array() && localClientUID != clientstr) {
					json::array posArray = jsonVal[clientid].as_array();
					TS3_VECTOR position;

					//printf("Setting position for %s to %f, %f, %f\n", clientstr.c_str(), posArray[0].as_double(), posArray[1].as_double(), posArray[2].as_double());
					position.x = -1 * posArray[0].as_double();
					position.y = posArray[1].as_double();
					position.z = posArray[2].as_double();

					ts3Functions.channelset3DAttributes(serverConnectionHandlerID, clientidlist[i], &position);

					//free(&posArray);
					//free(&forward);
					//free(&up);
				}
				else {
					if (!jsonVal[clientid].is_array() && localClientUID != clientstr) {
						//Player probably isn't registered
						TS3_VECTOR position;
						position.x = 0.0f;
						position.y = 0.0f;
						position.z = 0.0f;

						if (!CanHearUnregistered) {
							position.y = -1024.0f;
						}					
						
						ts3Functions.channelset3DAttributes(serverConnectionHandlerID, clientidlist[i], &position);

						printf("Player likely not registered. with id = %s\n", clientstr.c_str());
					}
					else if (localClientUID == clientstr) {
						//TS user is player
						json::array posArray = jsonVal[clientid].as_array();

						double pitch = 0.0f;//posArray[3].as_double();
						double yaw = posArray[4].as_double() + (3.14 * 1.5);

						double xzLen = cos(pitch);
						double x = xzLen * cos(yaw);
						double y = sin(pitch);
						double z = xzLen * sin(-yaw);

						TS3_VECTOR new_forward;
						new_forward.x = (float)x;
						new_forward.y = (float)y;
						new_forward.z = (float)z;

						ts3Functions.systemset3DSettings(serverConnectionHandlerID, 1.0f, 1.0f);
						ts3Functions.systemset3DListenerAttributes(serverConnectionHandlerID, &center, &new_forward, &up);
					}
				}

				//free(&clientstr);
				//free(&clientid);
				//ts3Functions.freeMemory(clientUID);
			}

		}

	}
	catch (const std::exception& e) {
		printf("An error occured or the connection timed out\n");
	}

	//free(&client);
	//free(&builder);
	//free(&response);
	//free(&jsonVal);
	//ts3Functions.freeMemory(clientidlist);
	//ts3Functions.freeMemory(&channelID);
}

#pragma endregion
/********************************** Required functions ************************************/
#pragma region RequiredFunctions

/* Unique name identifying this plugin */
const char* ts3plugin_name() {
	return "DPAR";
}

/* Plugin version */
const char* ts3plugin_version() {
    return "1.0";
}

/* Plugin API version. Must be the same as the clients API major version, else the plugin fails to load. */
int ts3plugin_apiVersion() {
	return PLUGIN_API_VERSION;
}

/* Plugin author */
const char* ts3plugin_author() {
	/* If you want to use wchar_t, see ts3plugin_name() on how to use */
    return "Darke Bros Dev Team";
}

/* Plugin description */
const char* ts3plugin_description() {
	/* If you want to use wchar_t, see ts3plugin_name() on how to use */
    return "Positional Audio Receiver";
}

/* Set TeamSpeak 3 callback functions */
void ts3plugin_setFunctionPointers(const struct TS3Functions funcs) {
    ts3Functions = funcs;
}

/*
 * Custom code called right after loading the plugin. Returns 0 on success, 1 on failure.
 * If the function returns 1 on failure, the plugin will be unloaded again.
 */
int ts3plugin_init() {
    char appPath[PATH_BUFSIZE];
    char resourcesPath[PATH_BUFSIZE];
    char configPath[PATH_BUFSIZE];
	char pluginPath[PATH_BUFSIZE];

    /* Your plugin init code here */
    printf("PLUGIN: init\n");

    /* Example on how to query application, resources and configuration paths from client */
    /* Note: Console client returns empty string for app and resources path */
    ts3Functions.getAppPath(appPath, PATH_BUFSIZE);
    ts3Functions.getResourcesPath(resourcesPath, PATH_BUFSIZE);
    ts3Functions.getConfigPath(configPath, PATH_BUFSIZE);
	ts3Functions.getPluginPath(pluginPath, PATH_BUFSIZE, pluginID);

	//These probably aren't required anymore...
	//SetDllDirectoryA(std::string(pluginPath).append("\\CPP-PAR\\").c_str());
	//LoadLibraryA(std::string(pluginPath).append("\\CPP-PAR\\cpprest_2_10d.dll").c_str());

	printf("PLUGIN: App path: %s\nResources path: %s\nConfig path: %s\nPlugin path: %s\n", appPath, resourcesPath, configPath, pluginPath);

	t = Timer();

    return 0;  /* 0 = success, 1 = failure, -2 = failure but client will not show a "failed to load" warning */
	/* -2 is a very special case and should only be used if a plugin displays a dialog (e.g. overlay) asking the user to disable
	 * the plugin again, avoiding the show another dialog by the client telling the user the plugin failed to load.
	 * For normal case, if a plugin really failed to load because of an error, the correct return value is 1. */
}

/* Custom code called right before the plugin is unloaded */
void ts3plugin_shutdown() {
    /* Your plugin cleanup code here */
    printf("PLUGIN: shutdown\n");

	/*
	 * Note:
	 * If your plugin implements a settings dialog, it must be closed and deleted here, else the
	 * TeamSpeak client will most likely crash (DLL removed but dialog from DLL code still open).
	 */

	/* Free pluginID if we registered it */
	if(pluginID) {
		free(pluginID);
		pluginID = NULL;
	}
}
#pragma endregion
/********************************** Optional functions ************************************/
#pragma region OptionalFunctions
/*
 * Following functions are optional, if not needed you don't need to implement them.
 */

/* Tell client if plugin offers a configuration window. If this function is not implemented, it's an assumed "does not offer" (PLUGIN_OFFERS_NO_CONFIGURE). */
int ts3plugin_offersConfigure() {
	/*
	 * Return values:
	 * PLUGIN_OFFERS_NO_CONFIGURE         - Plugin does not implement ts3plugin_configure
	 * PLUGIN_OFFERS_CONFIGURE_NEW_THREAD - Plugin does implement ts3plugin_configure and requests to run this function in an own thread
	 * PLUGIN_OFFERS_CONFIGURE_QT_THREAD  - Plugin does implement ts3plugin_configure and requests to run this function in the Qt GUI thread
	 */
	return PLUGIN_OFFERS_NO_CONFIGURE;  /* In this case ts3plugin_configure does not need to be implemented */
}

/*
 * If the plugin wants to use error return codes, plugin commands, hotkeys or menu items, it needs to register a command ID. This function will be
 * automatically called after the plugin was initialized. This function is optional. If you don't use these features, this function can be omitted.
 * Note the passed pluginID parameter is no longer valid after calling this function, so you must copy it and store it in the plugin.
 */
void ts3plugin_registerPluginID(const char* id) {
	const size_t sz = strlen(id) + 1;
	pluginID = (char*)malloc(sz * sizeof(char));
	_strcpy(pluginID, sz, id);  /* The id buffer will invalidate after exiting this function */
	printf("PLUGIN: registerPluginID: %s\n", pluginID);
}

/* Required to release the memory for parameter "data" allocated in ts3plugin_infoData and ts3plugin_initMenus */
void ts3plugin_freeMemory(void* data) {
	free(data);
}

/*
 * Plugin requests to be always automatically loaded by the TeamSpeak 3 client unless
 * the user manually disabled it in the plugin dialog.
 * This function is optional. If missing, no autoload is assumed.
 */
int ts3plugin_requestAutoload() {
	return 1;  /* 1 = request autoloaded, 0 = do not request autoload */
}

/* Helper function to create a menu item */
static struct PluginMenuItem* createMenuItem(enum PluginMenuType type, int id, const char* text, const char* icon) {
	struct PluginMenuItem* menuItem = (struct PluginMenuItem*)malloc(sizeof(struct PluginMenuItem));
	menuItem->type = type;
	menuItem->id = id;
	_strcpy(menuItem->text, PLUGIN_MENU_BUFSZ, text);
	_strcpy(menuItem->icon, PLUGIN_MENU_BUFSZ, icon);
	return menuItem;
}

/* Some makros to make the code to create menu items a bit more readable */
#define BEGIN_CREATE_MENUS(x) const size_t sz = x + 1; size_t n = 0; *menuItems = (struct PluginMenuItem**)malloc(sizeof(struct PluginMenuItem*) * sz);
#define CREATE_MENU_ITEM(a, b, c, d) (*menuItems)[n++] = createMenuItem(a, b, c, d);
#define END_CREATE_MENUS (*menuItems)[n++] = NULL; assert(n == sz);

/*
 * Menu IDs for this plugin. Pass these IDs when creating a menuitem to the TS3 client. When the menu item is triggered,
 * ts3plugin_onMenuItemEvent will be called passing the menu ID of the triggered menu item.
 * These IDs are freely choosable by the plugin author. It's not really needed to use an enum, it just looks prettier.
 */
enum {
	MENU_ID_CHANNEL_ENABLE,
	MENU_ID_CHANNEL_DISABLE,
	MENU_ID_GLOBAL_ENABLE,
	MENU_ID_GLOBAL_DISABLE,
	MENU_ID_REFRESH_CONFIGURATION
};

/*
 * Initialize plugin menus.
 * This function is called after ts3plugin_init and ts3plugin_registerPluginID. A pluginID is required for plugin menus to work.
 * Both ts3plugin_registerPluginID and ts3plugin_freeMemory must be implemented to use menus.
 * If plugin menus are not used by a plugin, do not implement this function or return NULL.
 */
void ts3plugin_initMenus(struct PluginMenuItem*** menuItems, char** menuIcon) {
	/*
	 * Create the menus
	 * There are three types of menu items:
	 * - PLUGIN_MENU_TYPE_CLIENT:  Client context menu
	 * - PLUGIN_MENU_TYPE_CHANNEL: Channel context menu
	 * - PLUGIN_MENU_TYPE_GLOBAL:  "Plugins" menu in menu bar of main window
	 *
	 * Menu IDs are used to identify the menu item when ts3plugin_onMenuItemEvent is called
	 *
	 * The menu text is required, max length is 128 characters
	 *
	 * The icon is optional, max length is 128 characters. When not using icons, just pass an empty string.
	 * Icons are loaded from a subdirectory in the TeamSpeak client plugins folder. The subdirectory must be named like the
	 * plugin filename, without dll/so/dylib suffix
	 * e.g. for "test_plugin.dll", icon "1.png" is loaded from <TeamSpeak 3 Client install dir>\plugins\test_plugin\1.png
	 */

	BEGIN_CREATE_MENUS(3);  /* IMPORTANT: Number of menu items must be correct! */
	//CREATE_MENU_ITEM(PLUGIN_MENU_TYPE_CHANNEL, MENU_ID_CHANNEL_ENABLE, "Enable on this channel", "1.png");
	//CREATE_MENU_ITEM(PLUGIN_MENU_TYPE_CHANNEL, MENU_ID_CHANNEL_DISABLE, "Disable on this channel", "2.png");
	CREATE_MENU_ITEM(PLUGIN_MENU_TYPE_GLOBAL,  MENU_ID_GLOBAL_ENABLE,  "Enable Globally",  "1.png");
	CREATE_MENU_ITEM(PLUGIN_MENU_TYPE_GLOBAL,  MENU_ID_GLOBAL_DISABLE,  "Disable Globally",  "2.png");
	CREATE_MENU_ITEM(PLUGIN_MENU_TYPE_GLOBAL, MENU_ID_REFRESH_CONFIGURATION, "Refresh configuration", "3.png");
	END_CREATE_MENUS;  /* Includes an assert checking if the number of menu items matched */

	/*
	 * Specify an optional icon for the plugin. This icon is used for the plugins submenu within context and main menus
	 * If unused, set menuIcon to NULL
	 */
	*menuIcon = (char*)malloc(PLUGIN_MENU_BUFSZ * sizeof(char));
	_strcpy(*menuIcon, PLUGIN_MENU_BUFSZ, "t.png");

	/*
	 * Menus can be enabled or disabled with: ts3Functions.setPluginMenuEnabled(pluginID, menuID, 0|1);
	 * Test it with plugin command: /test enablemenu <menuID> <0|1>
	 * Menus are enabled by default. Please note that shown menus will not automatically enable or disable when calling this function to
	 * ensure Qt menus are not modified by any thread other the UI thread. The enabled or disable state will change the next time a
	 * menu is displayed.
	 */
	/* For example, this would disable MENU_ID_GLOBAL_2: */
	/* ts3Functions.setPluginMenuEnabled(pluginID, MENU_ID_GLOBAL_2, 0); */

	/* All memory allocated in this function will be automatically released by the TeamSpeak client later by calling ts3plugin_freeMemory */
}
#pragma endregion
/********************************** TeamSpeak callbacks ***********************************/
#pragma region TS3Callbacks
/*
 * Following functions are optional, feel free to remove unused callbacks.
 * See the clientlib documentation for details on each function.
 */

/* Clientlib */

void ts3plugin_onConnectStatusChangeEvent(uint64 serverConnectionHandlerID, int newStatus, unsigned int errorNumber) {
    /* Some example code following to show how to use the information query functions. */

    if(newStatus == STATUS_CONNECTION_ESTABLISHED) {  /* connection established and we have client and channels available */
        char* s;
        char msg[1024];
        anyID myID;
        uint64* ids;
        size_t i;
		unsigned int error;

        /* Print clientlib version */
        if(ts3Functions.getClientLibVersion(&s) == ERROR_ok) {
            printf("PLUGIN: Client lib version: %s\n", s);
            ts3Functions.freeMemory(s);  /* Release string */
        } else {
            ts3Functions.logMessage("Error querying client lib version", LogLevel_ERROR, "DPAR", serverConnectionHandlerID);
            return;
        }

		/* Write plugin name and version to log */
        snprintf(msg, sizeof(msg), "Plugin %s, Version %s, Author: %s", ts3plugin_name(), ts3plugin_version(), ts3plugin_author());
        ts3Functions.logMessage(msg, LogLevel_INFO, "DPAR", serverConnectionHandlerID);

        /* Print virtual server name */
        if((error = ts3Functions.getServerVariableAsString(serverConnectionHandlerID, VIRTUALSERVER_NAME, &s)) != ERROR_ok) {
			if(error != ERROR_not_connected) {  /* Don't spam error in this case (failed to connect) */
				ts3Functions.logMessage("Error querying server name", LogLevel_ERROR, "DPAR", serverConnectionHandlerID);
			}
            return;
        }
        printf("PLUGIN: Server name: %s\n", s);
        ts3Functions.freeMemory(s);

        /* Print virtual server welcome message */
        if(ts3Functions.getServerVariableAsString(serverConnectionHandlerID, VIRTUALSERVER_WELCOMEMESSAGE, &s) != ERROR_ok) {
            ts3Functions.logMessage("Error querying server welcome message", LogLevel_ERROR, "DPAR", serverConnectionHandlerID);
            return;
        }
        printf("PLUGIN: Server welcome message: %s\n", s);
        ts3Functions.freeMemory(s);  /* Release string */

        /* Print own client ID and nickname on this server */
        if(ts3Functions.getClientID(serverConnectionHandlerID, &myID) != ERROR_ok) {
            ts3Functions.logMessage("Error querying client ID", LogLevel_ERROR, "DPAR", serverConnectionHandlerID);
            return;
        }
        if(ts3Functions.getClientSelfVariableAsString(serverConnectionHandlerID, CLIENT_NICKNAME, &s) != ERROR_ok) {
            ts3Functions.logMessage("Error querying client nickname", LogLevel_ERROR, "DPAR", serverConnectionHandlerID);
            return;
        }
        printf("PLUGIN: My client ID = %d, nickname = %s\n", myID, s);
        ts3Functions.freeMemory(s);

        /* Print list of all channels on this server */
        if(ts3Functions.getChannelList(serverConnectionHandlerID, &ids) != ERROR_ok) {
            ts3Functions.logMessage("Error getting channel list", LogLevel_ERROR, "DPAR", serverConnectionHandlerID);
            return;
        }
        printf("PLUGIN: Available channels:\n");
        for(i=0; ids[i]; i++) {
            /* Query channel name */
            if(ts3Functions.getChannelVariableAsString(serverConnectionHandlerID, ids[i], CHANNEL_NAME, &s) != ERROR_ok) {
                ts3Functions.logMessage("Error querying channel name", LogLevel_ERROR, "DPAR", serverConnectionHandlerID);
                return;
            }
            printf("PLUGIN: Channel ID = %llu, name = %s\n", (long long unsigned int)ids[i], s);
            ts3Functions.freeMemory(s);
        }
        ts3Functions.freeMemory(ids);  /* Release array */

        /* Print list of existing server connection handlers */
        printf("PLUGIN: Existing server connection handlers:\n");
        if(ts3Functions.getServerConnectionHandlerList(&ids) != ERROR_ok) {
            ts3Functions.logMessage("Error getting server list", LogLevel_ERROR, "DPAR", serverConnectionHandlerID);
            return;
        }
        for(i=0; ids[i]; i++) {
            if((error = ts3Functions.getServerVariableAsString(ids[i], VIRTUALSERVER_NAME, &s)) != ERROR_ok) {
				if(error != ERROR_not_connected) {  /* Don't spam error in this case (failed to connect) */
					ts3Functions.logMessage("Error querying server name", LogLevel_ERROR, "DPAR", serverConnectionHandlerID);
				}
                continue;
            }
            printf("- %llu - %s\n", (long long unsigned int)ids[i], s);
            ts3Functions.freeMemory(s);
        }
        ts3Functions.freeMemory(ids);
    }
}

void ts3plugin_onUpdateChannelEditedEvent(uint64 serverConnectionHandlerID, uint64 channelID, anyID invokerID, const char* invokerName, const char* invokerUniqueIdentifier) {
	// TODO: Check if channel gets PosAudio Config

	// Get the channel id of the channel we're in
	uint64 currentChannelID = dpar_getMyCurrentChannel(serverConnectionHandlerID);

	if (channelID == currentChannelID) {

		ts3Functions.logMessage("Channel we are in was edited", LogLevel_INFO, "DPAR", serverConnectionHandlerID);

		t.stop();
		printf("DPAR: Kill timer\n");

		dpar_updateConfigFromChannelDescription(serverConnectionHandlerID, channelID);

		if (ChannelHasConfig) {
			// Attempt to get new parameters from the remote
			dpar_updateFromRemoteConfiguration(serverConnectionHandlerID);

			// Update the 3D positions of clients without delay
			dpar_update3Dposition(serverConnectionHandlerID);

			// Schedule updates on an interval
			t.setTimeout(&dpar_setIntervalForTimer, serverConnectionHandlerID, 500);
		}
	}
}

void ts3plugin_onClientMoveEvent(uint64 serverConnectionHandlerID, anyID clientID, uint64 oldChannelID, uint64 newChannelID, int visibility, const char* moveMessage) {

	

	printf("ts3plugin_onClientMoveEvent: %d %d %d %d %d %s\n", serverConnectionHandlerID, clientID, oldChannelID, newChannelID, visibility, moveMessage);

	anyID myID;
	if (ts3Functions.getClientID(serverConnectionHandlerID, &myID) != ERROR_ok) {
		ts3Functions.logMessage("Error querying client ID", LogLevel_ERROR, "DPAR", serverConnectionHandlerID);
		return;
	}

	printf("ts3plugin_onClientMoveEvent IDs: %d %d\n", myID, clientID);

	cout << &t << endl;

	if (myID == clientID) {
		// We moved channel so should stop updating positions until we can establish the channel's config
		t.stop();
		printf("DPAR: Kill timer\n");
		dpar_updateConfigFromChannelDescription(serverConnectionHandlerID, newChannelID);

		if (ChannelHasConfig) {
			// Attempt to get new parameters from the remote
			dpar_updateFromRemoteConfiguration(serverConnectionHandlerID);

			// Update the 3D positions of clients instantly
			dpar_update3Dposition(serverConnectionHandlerID);

			// Schedule updates on an interval
			t.setTimeout(&dpar_setIntervalForTimer, serverConnectionHandlerID, 500);
		}
	}
}

void ts3plugin_onClientMoveMovedEvent(uint64 serverConnectionHandlerID, anyID clientID, uint64 oldChannelID, uint64 newChannelID, int visibility, anyID moverID, const char* moverName, const char* moverUniqueIdentifier, const char* moveMessage) {
	// TODO: Similar activations to ts3plugin_onClientMoveEvent
	cout << "ts3plugin_onClientMoveMovedEvent" << endl;
}

int ts3plugin_onServerErrorEvent(uint64 serverConnectionHandlerID, const char* errorMessage, unsigned int error, const char* returnCode, const char* extraMessage) {
	printf("PLUGIN: onServerErrorEvent %llu %s %d %s\n", (long long unsigned int)serverConnectionHandlerID, errorMessage, error, (returnCode ? returnCode : ""));
	if(returnCode) {
		/* A plugin could now check the returnCode with previously (when calling a function) remembered returnCodes and react accordingly */
		/* In case of using a a plugin return code, the plugin can return:
		 * 0: Client will continue handling this error (print to chat tab)
		 * 1: Client will ignore this error, the plugin announces it has handled it */
		return 1;
	}
	return 0;  /* If no plugin return code was used, the return value of this function is ignored */
}

void ts3plugin_onChannelDescriptionUpdateEvent(uint64 serverConnectionHandlerID, uint64 channelID) {
	// TODO: Update config from updated channel if currently in that channel
	cout << "ts3plugin_onChannelDescriptionUpdateEvent" << endl;
}

void ts3plugin_onCustom3dRolloffCalculationClientEvent(uint64 serverConnectionHandlerID, anyID clientID, float distance, float* volume) {
	std::string strdistance = "\nDistance of client:";
	strdistance = strdistance.append(std::to_string(distance));
	//printf(strdistance.c_str());
	std::string strvolume = "\nVolume of client:";
	strvolume = strvolume.append(std::to_string(*volume));
	//printf(strvolume.c_str());

	if (distance < RolloffOffset) {
		*volume = 1.0f;
	}
	else {
		//Insert in to Desmos: C:Steepness 0.1-1 step:0.1 B:Max range A-inf A:Offset 0-inf
		//1-\left(\frac{\left(\operatorname{abs}\left(x\right)-a\right)}{b-a}\right)^{c}\left\{\operatorname{abs}\left(x\right)>a\right\}
		//y=1\left\{\operatorname{abs}\left(x\right)<a\right\}

		float v = 1.0f - pow(((distance - RolloffOffset)/(RolloffCutoff-RolloffOffset)),RolloffAttenuationCoefficient);
		if (v < 0.0f) {
			v = 0.0f;
		}
		*volume = v;
	}


	strdistance.erase();
	strvolume.erase();
}

void ts3plugin_onCustom3dRolloffCalculationWaveEvent(uint64 serverConnectionHandlerID, uint64 waveHandle, float distance, float* volume) {
	std::string strdistance = "\nDistance of client:";
	strdistance = strdistance.append(std::to_string(distance));
	//printf(strdistance.c_str());
	std::string strvolume = "\nVolume of client:";
	strvolume = strvolume.append(std::to_string(*volume));
	//printf(strvolume.c_str());

	strdistance.erase();
	strvolume.erase();
}

/*
 * Called when a plugin menu item (see ts3plugin_initMenus) is triggered. Optional function, when not using plugin menus, do not implement this.
 *
 * Parameters:
 * - serverConnectionHandlerID: ID of the current server tab
 * - type: Type of the menu (PLUGIN_MENU_TYPE_CHANNEL, PLUGIN_MENU_TYPE_CLIENT or PLUGIN_MENU_TYPE_GLOBAL)
 * - menuItemID: Id used when creating the menu item
 * - selectedItemID: Channel or Client ID in the case of PLUGIN_MENU_TYPE_CHANNEL and PLUGIN_MENU_TYPE_CLIENT. 0 for PLUGIN_MENU_TYPE_GLOBAL.
 */
void ts3plugin_onMenuItemEvent(uint64 serverConnectionHandlerID, enum PluginMenuType type, int menuItemID, uint64 selectedItemID) {
	printf("PLUGIN: onMenuItemEvent: serverConnectionHandlerID=%llu, type=%d, menuItemID=%d, selectedItemID=%llu\n", (long long unsigned int)serverConnectionHandlerID, type, menuItemID, (long long unsigned int)selectedItemID);
	char* Chname = NULL;
	switch(type) {
		case PLUGIN_MENU_TYPE_GLOBAL:
			/* Global menu item was triggered. selectedItemID is unused and set to zero. */
			switch(menuItemID) {
				case MENU_ID_GLOBAL_ENABLE:
					/* Menu global 1 was triggered */
					dpar_onPluginEnable(serverConnectionHandlerID);
					break;
				case MENU_ID_GLOBAL_DISABLE:
					/* Menu global 2 was triggered */
					dpar_onPluginDisable(serverConnectionHandlerID);
					break;
				case MENU_ID_REFRESH_CONFIGURATION:
					/* Menu global 2 was triggered */
					if (ChannelHasConfig) {
						dpar_updateFromRemoteConfiguration(serverConnectionHandlerID);
					}
					break;
				default:
					break;
			}
			break;
		default:
			break;
	}
	ts3Functions.freeMemory(Chname);
}
#pragma endregion