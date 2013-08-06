#include "pebble_os.h"
#include "pebble_app.h"
#include "pebble_fonts.h"
#include "http.h"
#include "pebble-housecontrol.h"

// #define MY_UUID { 0x5F, 0xCC, 0x5B, 0x08, 0xD3, 0x60, 0x4B, 0xBD, 0xB8, 0xD5, 0x68, 0xFB, 0x33, 0x55, 0x51, 0xCC }
PBL_APP_INFO(HTTP_UUID,
             "HouseControl", "Rodger Combs",
             1, 0, /* App version */
             RESOURCE_ID_IMAGE_MENU_ICON,
             APP_INFO_STANDARD_APP);

Window homeWindow;
Window groupWindow;
Window deviceWindow;

// GRAPHICS BY SCHWAL :D

HeapBitmap icon_light_on;
HeapBitmap icon_light_off;
HeapBitmap icon_lock_on;
HeapBitmap icon_lock_off;
HeapBitmap icon_other_on;
HeapBitmap icon_other_off;

MenuLayer homeLayer;
MenuLayer groupLayer;
ActionBarLayer deviceLayer;
TextLayer deviceNameLayer;

static char show_message[32] = "";

static HouseControlConfig config;

static WindowType currentWindow;

static uint8_t request_in_progress = 0;

static Room rooms[9];
static uint16_t room_count;
static uint8_t rooms_loaded = 0;
static uint8_t room_id = 0;

static Device devices[9];
static uint16_t device_count;
static uint8_t devices_loaded = 0;

static Device current_device;

HTTPResult send_request_queue(){
	HTTPResult result = http_out_send();
	if(result == HTTP_OK){
		request_in_progress = 1;
	}
	return result;
}

void show_error(char* message){
	APP_LOG(APP_LOG_LEVEL_ERROR, "SHOW_ERROR: %s", message);
	strcpy(&show_message[0], message);
	menu_layer_reload_data(&homeLayer);
	menu_layer_reload_data(&groupLayer);
}

void request_room_list(){
	DictionaryIterator *body;
	APP_LOG(APP_LOG_LEVEL_INFO, "REQUESTING ROOM LIST");
	HTTPResult result = http_out_get(config.url, REQUEST_ROOMS, &body);
	if(result != HTTP_OK) {
		char str[32];
		snprintf(str, 32, "H HOG %i", result);
		show_error(str);
		return;
	}
	dict_write_cstring(body, HTTP_KEY_PASSWORD, config.password);
	dict_write_int32(body, HTTP_KEY_TYPE, HTTP_TYPE_ROOMS);
	dict_write_int32(body, HTTP_KEY_OFFSET, rooms_loaded);
	result = send_request_queue();
	if(result != HTTP_OK){
		char str[32];
		snprintf(str, 32, "H HOS %i", result);
		show_error(str);
		return;
	}
}

void request_device_list(){
	DictionaryIterator *body;
	APP_LOG(APP_LOG_LEVEL_INFO, "REQUESTING DEVICE LIST");
	HTTPResult result = http_out_get(config.url, REQUEST_DEVICES, &body);
	if(result != HTTP_OK) {
		char str[32];
		snprintf(str, 32, "G HOG %i", result);
		show_error(str);
		return;
	}
	dict_write_cstring(body, HTTP_KEY_PASSWORD, config.password);
	dict_write_int32(body, HTTP_KEY_TYPE, HTTP_TYPE_DEVICES);
	dict_write_int32(body, HTTP_KEY_OFFSET, devices_loaded);
	dict_write_int32(body, HTTP_KEY_ID, room_id);
	result = send_request_queue();
	if(result != HTTP_OK){
		char str[32];
		snprintf(str, 32, "G HOS %i", result);
		show_error(str);
		return;
	}
}

void run_next_request(){
	if(currentWindow == WINDOW_HOME){
		if(room_count > rooms_loaded || room_count == 0){
			request_room_list();
		}
	}else if(currentWindow == WINDOW_ROOM){
		if(device_count > devices_loaded || device_count == 0){
			request_device_list();
		}
	}
}

void handle_http_success(int32_t cookie, int http_status, DictionaryIterator* response, void* context){
	request_in_progress = 0;
	if(http_status != 200){
		char status[20] = "";
		snprintf(status, 20, "H SUC %i", http_status);
		show_error(status);
		return;
	}
	if(cookie == REQUEST_ROOMS){
		Tuple* count_tuple = dict_find(response, HTTP_KEY_COUNT);
		if(!count_tuple){
			show_error("NO COUNT ROOMS");
			return;
		}
		room_count = count_tuple->value->int16;
		if(room_count > 9){
			room_count = 9;
		}
		Tuple* data_tuple = dict_find(response, HTTP_KEY_DATA);
		if(!data_tuple){
			show_error("NO DATA ROOMS");
			return;
		}
		uint16_t new_rooms = data_tuple->length / sizeof(Room);
		uint16_t cpyLength = data_tuple->length;
		if(cpyLength + sizeof(Room) * rooms_loaded > sizeof(rooms)){
			cpyLength = sizeof(rooms) - sizeof(Room) * rooms_loaded;
		}
		memcpy(&rooms[rooms_loaded], &(data_tuple->value->data[0]), cpyLength);
		rooms_loaded += new_rooms;
		menu_layer_reload_data(&homeLayer);
	}else if(cookie == REQUEST_DEVICES){
		Tuple* count_tuple = dict_find(response, HTTP_KEY_COUNT);
		if(!count_tuple){
			show_error("NO COUNT DEVICES");
			return;
		}
		device_count = count_tuple->value->int16;
		if(device_count > 9){
			device_count = 9;
		}
		Tuple* data_tuple = dict_find(response, HTTP_KEY_DATA);
		if(!data_tuple){
			show_error("NO DATA DEVICES");
			return;
		}
		uint16_t new_devices = data_tuple->length / sizeof(Device);
		uint16_t cpyLength = data_tuple->length;
		if(cpyLength + sizeof(Device) * devices_loaded > sizeof(devices)){
			cpyLength = sizeof(devices) - sizeof(Device) * devices_loaded;
		}
		memcpy(&devices[devices_loaded], &(data_tuple->value->data[0]), cpyLength);
		devices_loaded += new_devices;
		menu_layer_reload_data(&groupLayer);
	}
	run_next_request();
}

void handle_http_failure(int32_t cookie, int http_status, void* context){
	request_in_progress = 0;
	char status[20] = "";
	snprintf(status, 20, "H FAI %i", http_status);
	show_error(status);
}

void handle_home_window_appear(struct Window *window){
	if(currentWindow == WINDOW_HOME){
		rooms_loaded = 0;
		room_count = 0;
	}
	currentWindow = WINDOW_HOME;
	if(rooms_loaded < room_count || rooms_loaded == 0){
		if(!request_in_progress){
			request_room_list();
		}
	}
}

void handle_group_window_appear(struct Window *window){
	if(currentWindow == WINDOW_HOME){
		device_count = 0;
		devices_loaded = 0;
		menu_layer_set_selected_index(&groupLayer, MenuIndex(0, 0), MenuRowAlignTop, false);
	}
	currentWindow = WINDOW_ROOM;
	if(devices_loaded < device_count || devices_loaded == 0){
		if(!request_in_progress){
			request_device_list();
		}
	}
}

void handle_device_window_appear(struct Window *window){
	currentWindow = WINDOW_DEVICE;
	text_layer_set_text(&deviceNameLayer, &(current_device.name[0]));
//	GSize layer_size = text_layer_get_max_used_size(&deviceNameLayer, app_get_current_graphics_context());
	//FIXME:Calculate bounds for text layer here!
	if(current_device.type == DEVICE_LIGHT){
		action_bar_layer_set_icon(&deviceLayer, BUTTON_ID_UP, &icon_light_on.bmp);
		action_bar_layer_set_icon(&deviceLayer, BUTTON_ID_DOWN, &icon_light_off.bmp);
	}else if(current_device.type == DEVICE_LOCK){
		action_bar_layer_set_icon(&deviceLayer, BUTTON_ID_UP, &icon_lock_on.bmp);
		action_bar_layer_set_icon(&deviceLayer, BUTTON_ID_DOWN, &icon_lock_off.bmp);
	}else{
		action_bar_layer_set_icon(&deviceLayer, BUTTON_ID_UP, &icon_other_on.bmp);
		action_bar_layer_set_icon(&deviceLayer, BUTTON_ID_DOWN, &icon_other_off.bmp);
	}
}

uint16_t menu_home_get_num_rows(struct MenuLayer *menu_layer, uint16_t section_index, void *callback_context){
	if(rooms_loaded){
		return room_count;
	}else{
		return 1;
	}
}

void menu_home_draw_row(GContext *ctx, const Layer *cell_layer, MenuIndex *cell_index, void *callback_context){
	if(!rooms_loaded || rooms_loaded <= cell_index->row){
		menu_cell_basic_draw(ctx, cell_layer, "Loading...", NULL, NULL);
		return;
	}
	menu_cell_basic_draw(ctx, cell_layer, &(rooms[cell_index->row].name[0]), NULL, NULL);
}

void menu_home_select_click(struct MenuLayer *menu_layer, MenuIndex *cell_index, void *callback_context){
	if(cell_index->row > rooms_loaded){
		return;
	}
	room_id = rooms[cell_index->row].id;
	window_stack_push(&groupWindow, true);
}

uint16_t menu_group_get_num_rows(struct MenuLayer *menu_layer, uint16_t section_index, void *callback_context){
	if(devices_loaded){
		return device_count;
	}else{
		return 1;
	}
}

void menu_group_draw_row(GContext *ctx, const Layer *cell_layer, MenuIndex *cell_index, void *callback_context){
	if(!devices_loaded || devices_loaded <= cell_index->row){
		menu_cell_basic_draw(ctx, cell_layer, "Loading...", NULL, NULL);
		return;
	}
	GBitmap* icon;
	if(devices[cell_index->row].type == DEVICE_LIGHT){
		icon = &icon_light_on.bmp;
	}else if(devices[cell_index->row].type == DEVICE_LOCK){
		icon = &icon_lock_off.bmp;
	}else{
		icon = NULL;
	}
	menu_cell_basic_draw(ctx, cell_layer, &(devices[cell_index->row].name[0]), NULL, icon);
}

void menu_group_select_click(struct MenuLayer *menu_layer, MenuIndex *cell_index, void *callback_context){
	if(cell_index->row > devices_loaded){
		return;
	}
	current_device = devices[cell_index->row];
	window_stack_push(&deviceWindow, true);
}

void device_send_command(uint16_t dev_id, uint8_t major, uint8_t minor){
	DictionaryIterator *body;
	APP_LOG(APP_LOG_LEVEL_INFO, "REQUESTING DEVICE ACTION");
	HTTPResult result = http_out_get(config.url, REQUEST_DEVICE_ACTION, &body);
	if(result != HTTP_OK) {
		char str[32];
		snprintf(str, 32, "C HOG %i", result);
		show_error(str);
		return;
	}
	dict_write_cstring(body, HTTP_KEY_PASSWORD, config.password);
	dict_write_int32(body, HTTP_KEY_TYPE, HTTP_TYPE_DEVICE_ACTION);
	dict_write_int32(body, HTTP_KEY_DEVID, dev_id);
	dict_write_uint8(body, HTTP_KEY_MAJOR, major);
	dict_write_uint8(body, HTTP_KEY_MINOR, minor);
	result = send_request_queue();
	if(result != HTTP_OK){
		char str[32];
		snprintf(str, 32, "C HOS %i", result);
		show_error(str);
		return;
	}
}

void current_device_send_command(uint8_t major, uint8_t minor){
	device_send_command(current_device.id, major, minor);
}

void device_up_click_handler(ClickRecognizerRef recognizer, Window *window){
	current_device_send_command(0x11, 0xFF);
}

void device_down_click_handler(ClickRecognizerRef recognizer, Window *window){
	current_device_send_command(0x13, 0x00);
}

void device_up_multi_click_handler(ClickRecognizerRef recognizer, Window *window){
	current_device_send_command(0x12, 0xFF);
}

void device_down_multi_click_handler(ClickRecognizerRef recognizer, Window *window){
	current_device_send_command(0x14, 0x00);
}

void device_up_long_click_handler(ClickRecognizerRef recognizer, Window *window){
	current_device_send_command(0x17, 0x01);
}

void device_up_long_click_release_handler(ClickRecognizerRef recognizer, Window *window){
	current_device_send_command(0x18, 0x00);
}

void device_down_long_click_handler(ClickRecognizerRef recognizer, Window *window){
	current_device_send_command(0x17, 0x00);
}

void device_down_long_click_release_handler(ClickRecognizerRef recognizer, Window *window){
	current_device_send_command(0x18, 0x00);
}

void device_click_config_provider(ClickConfig **config, void *context){
	config[BUTTON_ID_UP]->click.handler = (ClickHandler)device_up_click_handler;
	config[BUTTON_ID_DOWN]->click.handler = (ClickHandler)device_down_click_handler;
	config[BUTTON_ID_UP]->long_click.handler = (ClickHandler)device_up_long_click_handler;
	config[BUTTON_ID_UP]->long_click.release_handler = (ClickHandler)device_up_long_click_release_handler;
	config[BUTTON_ID_UP]->long_click.delay_ms = 500;
	config[BUTTON_ID_DOWN]->long_click.handler = (ClickHandler)device_down_long_click_handler;
	config[BUTTON_ID_DOWN]->long_click.release_handler = (ClickHandler)device_down_long_click_release_handler;
	config[BUTTON_ID_DOWN]->long_click.delay_ms = 500;
	config[BUTTON_ID_UP]->multi_click.handler = (ClickHandler)device_up_multi_click_handler;
	config[BUTTON_ID_UP]->multi_click.min = 2;
	config[BUTTON_ID_UP]->multi_click.max = 2;
	config[BUTTON_ID_UP]->multi_click.last_click_only = true;
	config[BUTTON_ID_DOWN]->multi_click.handler = (ClickHandler)device_down_multi_click_handler;
	config[BUTTON_ID_DOWN]->multi_click.min = 2;
	config[BUTTON_ID_DOWN]->multi_click.max = 2;
	config[BUTTON_ID_DOWN]->multi_click.last_click_only = true;
}

void handle_init(AppContextRef ctx) {
	resource_init_current_app(&HOUSECONTROL);
	
	window_init(&homeWindow, "Group Selection");
	WindowHandlers handlers = {
		.appear = &handle_home_window_appear
	};
	window_set_window_handlers(&homeWindow, handlers);
	window_stack_push(&homeWindow, true /* Animated */);
	
	window_init(&groupWindow, "Device Selection");
	WindowHandlers handlers2 = {
		.appear = &handle_group_window_appear
	};
	window_set_window_handlers(&groupWindow, handlers2);
	
	window_init(&deviceWindow, "Device");
	WindowHandlers handlers3 = {
		.appear = &handle_device_window_appear
	};
	window_set_window_handlers(&deviceWindow, handlers3);
	
	//HOME LAYER
	GRect bounds = homeWindow.layer.bounds;
	menu_layer_init(&homeLayer, bounds);
	
	menu_layer_set_callbacks(&homeLayer, NULL, (MenuLayerCallbacks){
		.get_num_rows		= menu_home_get_num_rows,
		.draw_row 			= menu_home_draw_row,
		.select_click 		= menu_home_select_click
	});
	
	// Bind the menu layer's click config provider to the window for interactivity
	menu_layer_set_click_config_onto_window(&homeLayer, &homeWindow);
	
	// Add it to the window for display
	layer_add_child(&homeWindow.layer, menu_layer_get_layer(&homeLayer));
	
	//GROUP LAYER
	bounds = groupWindow.layer.bounds;
	menu_layer_init(&groupLayer, bounds);
	
	menu_layer_set_callbacks(&groupLayer, NULL, (MenuLayerCallbacks){
		.get_num_rows		= menu_group_get_num_rows,
		.draw_row 			= menu_group_draw_row,
		.select_click 		= menu_group_select_click
	});
	
	// Bind the menu layer's click config provider to the window for interactivity
	menu_layer_set_click_config_onto_window(&groupLayer, &groupWindow);
	
	// Add it to the window for display
	layer_add_child(&groupWindow.layer, menu_layer_get_layer(&groupLayer));
	
	//DEVICE LAYER
	action_bar_layer_init(&deviceLayer);
	
	action_bar_layer_set_click_config_provider(&deviceLayer, device_click_config_provider);
	
	// Add it to the window for display
	action_bar_layer_add_to_window(&deviceLayer, &deviceWindow);
	
	text_layer_init(&deviceNameLayer, GRect(0, 30, 124, 150));
	text_layer_set_font(&deviceNameLayer, fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD));
	text_layer_set_overflow_mode(&deviceNameLayer, GTextOverflowModeWordWrap);
	
	layer_insert_above_sibling((Layer *)&deviceNameLayer, (Layer *)&deviceLayer);
	
	ResHandle config_file = resource_get_handle(RESOURCE_ID_CONFIG_FILE);
	size_t config_size = resource_size(config_file);
	resource_load(config_file, (uint8_t*) &config, config_size);
	
	currentWindow = WINDOW_HOME;
	
	http_set_app_id(HTTP_APP_ID);
	
	HTTPCallbacks callbacks = {
		.success = &handle_http_success,
		.failure = &handle_http_failure
	};
	
	http_register_callbacks(callbacks, (void*)ctx);
	
	heap_bitmap_init(&icon_light_on, RESOURCE_ID_IMAGE_LIGHT_ON);
	heap_bitmap_init(&icon_light_off, RESOURCE_ID_IMAGE_LIGHT_OFF);
	heap_bitmap_init(&icon_lock_on, RESOURCE_ID_IMAGE_LOCK_ON);
	heap_bitmap_init(&icon_lock_off, RESOURCE_ID_IMAGE_LOCK_OFF);
	heap_bitmap_init(&icon_other_on, RESOURCE_ID_IMAGE_OTHER_ON);
	heap_bitmap_init(&icon_other_off, RESOURCE_ID_IMAGE_OTHER_OFF);
}


void pbl_main(void *params) {
	PebbleAppHandlers handlers = {
		.init_handler = &handle_init,
		.messaging_info = {
			.buffer_sizes = {
				.inbound = 124,
				.outbound = 512
			}
		}
	};
	app_event_loop(params, &handlers);
}
