#include <pebble.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#define KEY_TEMPERATURE 0
#define KEY_CONDITIONS 1
#define KEY_DAY 2

#define KEY_BT 3
#define KEY_Q_TIME 4
#define KEY_S_TIME 5
#define KEY_E_TIME 6


static Window *my_window;
static TextLayer *time_layer;
static TextLayer *date_layer;
static TextLayer *temp_layer;
static TextLayer *step_layer;
static TextLayer *bt_layer;
static TextLayer *battery_layer;
static GFont time_font_12;
static GFont time_font_18;
static GFont time_font_24;
static BitmapLayer *background_layer;
static GBitmap *background_bitmap;

static bool deadBattery = false;

static GColor textColor;
static GColor backGroundColor;

// Store incoming information
static char temperature_buffer[8];

bool btNotification = true;
bool triedToUpdateWeather = false;

bool quiteTime = false;

int startHour = 0;
int startMin = 0;
int endHour = 0;
int endMin = 0;

bool isQuiteTime = false;

static void update_steps(){
    HealthMetric metric = HealthMetricStepCount;
    time_t start = time_start_of_today();
    time_t end = time(NULL);

    // Check the metric has data available for today
    HealthServiceAccessibilityMask mask = health_service_metric_accessible(metric, start, end);

    if(mask & HealthServiceAccessibilityMaskAvailable) {
        // Data is available!
        APP_LOG(APP_LOG_LEVEL_INFO, "Steps today: %d", 
                (int)health_service_sum_today(metric));

        static char step_buffer[20];

        snprintf(step_buffer, sizeof(step_buffer), "%d", (int)health_service_sum_today(metric));

        text_layer_set_text(step_layer, step_buffer);
    } else {
        // No data recorded yet today    
        text_layer_set_text(step_layer, "0");
    }
}

static void update_weather(){
    triedToUpdateWeather = true;
    
    // Begin dictionary
    DictionaryIterator *iter;
    app_message_outbox_begin(&iter);

    // Add a key-value pair
    dict_write_uint8(iter, 0, 0);

    // Send the message!
    app_message_outbox_send();
}

static void health_handler(HealthEventType event, void *context) {    
    // Which type of event occurred?
    switch(event) {
        case HealthEventSignificantUpdate:
        break;
        case HealthEventMovementUpdate:
            update_steps();
        break;
        case HealthEventSleepUpdate:
        break;
    }
}

static void app_connection_handler(bool connected){
    if(connected){
        if(!isQuiteTime){
            APP_LOG(APP_LOG_LEVEL_INFO, "Not Quite Time");
            vibes_short_pulse();
        }
        
        text_layer_set_text_color(bt_layer, textColor);
    }else{
        if(!isQuiteTime){
            APP_LOG(APP_LOG_LEVEL_INFO, "Not Quite Time");
            vibes_short_pulse();
            vibes_short_pulse();
        }
        
        text_layer_set_text_color(bt_layer, GColorRed);
    }
}

static void update_time() {
    // Get a tm structure
    time_t temp = time(NULL);
    struct tm *tick_time = localtime(&temp);
    
    bool isAm = tick_time->tm_hour < 13;
    
    // Write the current hours and minutes into a buffer
    static char time_buffer[8];
    strftime(time_buffer, sizeof(time_buffer), clock_is_24h_style() ?
             "%H:%M" : isAm ? "%I:%MA" : "%I:%MP", tick_time);

    // Display this time on the TextLayer
    text_layer_set_text(time_layer, time_buffer);

    static char date_buffer[10];
    strftime(date_buffer, sizeof(date_buffer), "%m/%d/%y", tick_time);

    // Display this time on the TextLayer
    text_layer_set_text(date_layer, date_buffer);
    
    APP_LOG(APP_LOG_LEVEL_INFO, "Time Updated");
    
    //Are we runing in quite mode?
    if(quiteTime 
       && (startHour <= tick_time->tm_hour && tick_time->tm_hour <= endHour) 
       && (startMin <= tick_time->tm_min && tick_time->tm_min <= endMin)){
        APP_LOG(APP_LOG_LEVEL_INFO, "Updating QuiteTime to ON");
        isQuiteTime = true;
    }else{        
        APP_LOG(APP_LOG_LEVEL_INFO, "Updating QuiteTime to OFF");
        isQuiteTime = false;
    }
}

static void update_backgroundImage(int weatherId, bool isDay){
    gbitmap_destroy(background_bitmap);   
    /*
        0 = clear
        1 = cloud
        2 = partly
        3 = foggy
        4 = rain
        5 = thunder
    */
    
    // Assemble full string and display
    text_layer_set_text(temp_layer, temperature_buffer);
    if( weatherId == 0 ) {
        if(isDay){
            background_bitmap = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_SUNNY);
        } else {
            background_bitmap = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_MOON);
        }
    } else if( weatherId == 1 ) {
        background_bitmap = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_CLOUD);
    } else if( weatherId == 2 ) { 
        if(isDay){
            background_bitmap = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_PARTLY);    
        } else {
            background_bitmap = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_CLOUD_MOON);
        }
    } else if( weatherId == 3 ) {
        background_bitmap = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_FOGGY);
   } else if( weatherId == 4 ) {
        background_bitmap = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_RAINY); 
    } else if( weatherId == 5 ) {
        background_bitmap = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_THUNDER);
    } else {
        if(isDay){
            background_bitmap = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_SUNNY);
        } else {
            background_bitmap = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_MOON);
        }
    }

    bitmap_layer_set_bitmap(background_layer, background_bitmap);
}

static void handle_battery(BatteryChargeState charge_state) {
    static char battery_text[] = "100%";

    if (charge_state.is_charging && charge_state.charge_percent != 100) {
        snprintf(battery_text, sizeof(battery_text), "charging");
    } else if (charge_state.is_plugged){
        snprintf(battery_text, sizeof(battery_text), "full");      
    } else {
        snprintf(battery_text, sizeof(battery_text), "%d%%", charge_state.charge_percent);
    }

    text_layer_set_text(battery_layer, battery_text);

    if(charge_state.charge_percent <= 10){
        deadBattery = true;
        gbitmap_destroy(background_bitmap);
        background_bitmap = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_DEAD_BATTERY);
        bitmap_layer_set_bitmap(background_layer, background_bitmap);
    }else if(deadBattery){
        deadBattery = false;
        
        if(triedToUpdateWeather){
            update_weather();
        }
    }
}

static void tick_handler(struct tm *tick_time, TimeUnits units_changed) {    
    update_time();
    
    // Get weather update every 30 minutes
    if(tick_time->tm_min % 30 == 0 && !deadBattery) {        
        APP_LOG(APP_LOG_LEVEL_INFO, "Time TO update weather.");
        update_weather();
    }
}

static void set_layer_defaults(TextLayer *layer){
    text_layer_set_text_color(layer, textColor);
    text_layer_set_text_alignment(layer, GTextAlignmentCenter);
    text_layer_set_background_color(layer, GColorClear);
}

static void create_battery(GRect bounds){
    // Create temperature Layer
    battery_layer = text_layer_create(
        GRect(45, 0, bounds.size.w, 25));

    // Style the text
    text_layer_set_text(battery_layer, "...");
    set_layer_defaults(battery_layer);

    // Apply to TextLayer
    text_layer_set_font(battery_layer, time_font_18);
}

static void create_weather(GRect bounds){
    // Create temperature Layer
    temp_layer = text_layer_create(
        GRect(0, 0, 50, 25));

    // Style the text
    text_layer_set_text(temp_layer, "...");
    set_layer_defaults(temp_layer);

    // Apply to TextLayer
    text_layer_set_font(temp_layer, time_font_18);
}

static void create_time(GRect bounds){
    // Create the TextLayer with specific bounds
    // GRect(x,y,w,h)
    time_layer = text_layer_create(
        GRect(0, 110, bounds.size.w, 40));

    // Improve the layout to be more like a watchface
    text_layer_set_text(time_layer, "00:00");
    set_layer_defaults(time_layer);

    // Apply to TextLayer
    text_layer_set_font(time_layer, time_font_24);

    //Create the Date layer
    date_layer = text_layer_create(
        GRect(0, 145, bounds.size.w, 25));

    // Improve the layout to be more like a watchface
    text_layer_set_text(date_layer, "00/00/00");
    set_layer_defaults(date_layer);

    // Apply to TextLayer
    text_layer_set_font(date_layer, time_font_18);
}

static void create_step(GRect bounds){
    // Create temperature Layer
    step_layer = text_layer_create(
        GRect(0, 25, bounds.size.w, 25));

    // Style the text
    set_layer_defaults(step_layer);

    // Apply to TextLayer
    text_layer_set_font(step_layer, time_font_18);  

    #if defined(PBL_HEALTH)
    // Attempt to subscribe 
    if(!health_service_events_subscribe(health_handler, NULL)) {
        text_layer_set_text(step_layer, ":/");
    }
    #else
    text_layer_set_text(step_layer, "");
    #endif

    update_steps();
}

static void create_bt(GRect bounds){
    // Create temperature Layer
    bt_layer = text_layer_create(
        GRect(0, 0, bounds.size.w, 25));

    // Style the text
    set_layer_defaults(bt_layer);

    // Apply to TextLayer
    text_layer_set_text(bt_layer, "BT");
    text_layer_set_font(bt_layer, time_font_18);

    bool app_connection = connection_service_peek_pebble_app_connection();

    if(app_connection){
        text_layer_set_text_color(bt_layer, textColor);
    }else{    
        text_layer_set_text_color(bt_layer, GColorRed);
    }

    connection_service_subscribe((ConnectionHandlers) {
        .pebble_app_connection_handler = app_connection_handler
    });
}

static void create_backgroundLayor(GRect bounds){
    // Create BitmapLayer to display the GBitmap
    background_layer = bitmap_layer_create(GRect(0, 40, bounds.size.w, bounds.size.h/2));  

    // Set the bitmap onto the layer and add to the window
    bitmap_layer_set_compositing_mode(background_layer, GCompOpSet);
}

static void inbox_dropped_callback(AppMessageResult reason, void *context) {
    APP_LOG(APP_LOG_LEVEL_ERROR, "Message dropped!");
}

static void outbox_failed_callback(DictionaryIterator *iterator, AppMessageResult reason, void *context) {
    APP_LOG(APP_LOG_LEVEL_ERROR, "Outbox send failed!");
}

static void outbox_sent_callback(DictionaryIterator *iterator, void *context) {
    APP_LOG(APP_LOG_LEVEL_INFO, "Outbox send success!");
}

static void reload_config(){
    if(btNotification){
        APP_LOG(APP_LOG_LEVEL_INFO, "Turned BT on");
        text_layer_set_text(bt_layer, "BT");
        connection_service_subscribe((ConnectionHandlers) {
            .pebble_app_connection_handler = app_connection_handler
        });
    }else{
        APP_LOG(APP_LOG_LEVEL_INFO, "Turned BT off");
        text_layer_set_text(bt_layer, "");
        connection_service_unsubscribe();
    }
}

static void inbox_received_callback(DictionaryIterator *iterator, void *context) {
    // Read tuples for data
    Tuple *temp_tuple = dict_find(iterator, KEY_TEMPERATURE);
    Tuple *conditions_tuple = dict_find(iterator, KEY_CONDITIONS);
    Tuple *day_tuple = dict_find(iterator, KEY_DAY);
    
    Tuple *bt_note_tuble = dict_find(iterator, KEY_BT);    
    
    Tuple *q_time_tuble = dict_find(iterator, KEY_Q_TIME);
    Tuple *s_time_tuble = dict_find(iterator, KEY_S_TIME);
    Tuple *e_time_tuble = dict_find(iterator, KEY_E_TIME);

    if(q_time_tuble) {
        APP_LOG(APP_LOG_LEVEL_INFO, "Start Q: %s", q_time_tuble->value->cstring);
        
        quiteTime = strcmp(q_time_tuble->value->cstring, "1") == 0;
    }
    
    if(s_time_tuble && quiteTime) {
        char *startQTime = s_time_tuble->value->cstring;
        char hour[3];
        char min[3];
        
        APP_LOG(APP_LOG_LEVEL_INFO, "Start Q Time: %s", startQTime);
        
        strncpy(hour, startQTime, 2);
        hour[2] = '\0';        
        APP_LOG(APP_LOG_LEVEL_INFO, "Start hour: %s", hour);
        
        startHour = atoi(hour);
        
        strncpy(min, startQTime+3, 2);
        min[2] = '\0';
        APP_LOG(APP_LOG_LEVEL_INFO, "Start min: %s", min);
        
        startMin = atoi(min);        
    }else{
        startHour = 0;
        startMin = 0;
    }
    
    if(e_time_tuble && quiteTime) {
        char *endQTime = e_time_tuble->value->cstring;
        char hour[3];
        char min[3];
        
        APP_LOG(APP_LOG_LEVEL_INFO, "End Q Time: %s", endQTime);
        
        strncpy(hour, endQTime, 2);
        hour[2] = '\0';
        APP_LOG(APP_LOG_LEVEL_INFO, "End hour: %s", hour);
        
        endHour = atoi(hour);
        
        strncpy(min, endQTime+3, 2);
        min[2] = '\0';
        APP_LOG(APP_LOG_LEVEL_INFO, "End min: %s", min);
        
        endMin = atoi(min);
    }else{
        endHour = 0;
        endMin = 0;
    }

    if(bt_note_tuble) {
        APP_LOG(APP_LOG_LEVEL_INFO, "BT Notification: %s", bt_note_tuble->value->cstring);
        btNotification = strcmp(bt_note_tuble->value->cstring, "1") == 0;
    }

    // App should now update to take the user's preferences into account
    reload_config();

    // If all data is available, use it
    if(temp_tuple && conditions_tuple && day_tuple) {    
        int weatherId = 800;
        bool isDay = false;
        
        triedToUpdateWeather = false;
        snprintf(temperature_buffer, sizeof(temperature_buffer), "%dF", (int)temp_tuple->value->int32);

        APP_LOG(APP_LOG_LEVEL_INFO, "Is day 1 no 0 yes: %d", (int)day_tuple->value->int32);

        weatherId = (int)conditions_tuple->value->int32;
        isDay = ((int)day_tuple->value->int32) == 0;

        APP_LOG(APP_LOG_LEVEL_INFO, "Weather Id: %dF", weatherId);  

        update_backgroundImage(weatherId, isDay);
    }
}

static void main_window_load(Window *window) {
    // Get information about the Window
    Layer *window_layer = window_get_root_layer(window);
    GRect bounds = layer_get_bounds(window_layer);  

    // Create GFont
    time_font_24 = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_DYSLIXIA_32));
    time_font_18 = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_DYSLIXIA_18));
    time_font_12 = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_DYSLIXIA_12));

    create_backgroundLayor(bounds);

    create_time(bounds);

    create_battery(bounds);

    create_weather(bounds);

    create_step(bounds);

    create_bt(bounds);

    // Add it as a child layer to the Window's root layer
    layer_add_child(window_layer, bitmap_layer_get_layer(background_layer));
    layer_add_child(window_layer, text_layer_get_layer(time_layer));
    layer_add_child(window_layer, text_layer_get_layer(date_layer));
    layer_add_child(window_layer, text_layer_get_layer(temp_layer));
    layer_add_child(window_layer, text_layer_get_layer(battery_layer));
    layer_add_child(window_layer, text_layer_get_layer(step_layer));
    layer_add_child(window_layer, text_layer_get_layer(bt_layer));
}

static void main_window_unload(Window *window) {
    // Destroy TextLayer
    text_layer_destroy(time_layer);
    text_layer_destroy(date_layer);
    text_layer_destroy(temp_layer);
    text_layer_destroy(bt_layer);
    text_layer_destroy(step_layer);
    text_layer_destroy(battery_layer);

    // Unload GFont
    fonts_unload_custom_font(time_font_24);
    fonts_unload_custom_font(time_font_18);
    fonts_unload_custom_font(time_font_12);

    // Destroy BitmapLayer
    bitmap_layer_destroy(background_layer);

    gbitmap_destroy(background_bitmap);
}

static void handle_init(void) {
    // Register callbacks
    app_message_register_inbox_received(inbox_received_callback);
    app_message_register_inbox_dropped(inbox_dropped_callback);
    app_message_register_outbox_failed(outbox_failed_callback);
    app_message_register_outbox_sent(outbox_sent_callback);

    // Open AppMessage
    app_message_open(1000, 1000);    
    
    my_window = window_create();

    window_set_background_color(my_window, backGroundColor);

    // Set handlers to manage the elements inside the Window
    window_set_window_handlers(my_window, (WindowHandlers) {
        .load = main_window_load,
        .unload = main_window_unload
    });

    window_stack_push(my_window, true);

    // Make sure the time is displayed from the start
    update_time();

    // Register with TickTimerService
    tick_timer_service_subscribe(MINUTE_UNIT, tick_handler);

    // Register with battery state service subscribe
    battery_state_service_subscribe(handle_battery);
    
    // Get Battery Info off the bat.
    handle_battery(battery_state_service_peek());
}

static void handle_deinit(void) {
    window_destroy(my_window);
}

int main(void) {
    textColor = GColorWhite;
    backGroundColor = GColorBlack;

    handle_init();
    app_event_loop();
    handle_deinit();
}