#include <pebble.h>

#define LAST_ID_KEY 1

static Window *window;
static Layer *qr_layer;
static Tuple *qr_tuple;
static TextLayer *description_layer;
static AppTimer *query_timer;
//static AppTimer *descr_timer;
static int id;
static bool connected=true;
static bool first=true;
static bool is_empty[6]={false,false,false,false,false,false};
#ifdef PBL_ROUND
#define TEXT_OFFSET_Y 90
#define SCREEN_WIDTH 180
#else
#define TEXT_OFFSET_Y 144
#define SCREEN_WIDTH 144
#endif

enum {
  QUOTE_KEY_QRCODE = 0x0,
  QUOTE_KEY_FETCH = 0x1,
  QUOTE_KEY_DESCRIPTION = 0x2,
  QUOTE_KEY_ID=0x3
};

static bool send_to_phone_multi(int quote_key, int symbol) {
  
  // Loadinating
  //layer_set_hidden(text_layer_get_layer(description_layer), false);
  
  DictionaryIterator *iter;
  app_message_outbox_begin(&iter);

  Tuplet tuple = TupletInteger(quote_key, symbol);
  dict_write_tuplet(iter, &tuple);
  dict_write_end(iter);

  app_message_outbox_send();
  return true;
}

static void send_query(void* data)
{
  send_to_phone_multi(QUOTE_KEY_FETCH, id+1);
}

static void in_received_handler(DictionaryIterator *iter, void *context) {
  //APP_LOG(APP_LOG_LEVEL_DEBUG,"Got something");
  if(first)
  {
    first = false;
    //APP_LOG(APP_LOG_LEVEL_DEBUG,"Request data");
    // Send query again in one second
    query_timer=app_timer_register(100,send_query,NULL);
  }
  qr_tuple = dict_find(iter, QUOTE_KEY_QRCODE);
  if (qr_tuple) {
    //APP_LOG(APP_LOG_LEVEL_DEBUG,"Length of String: %d",qr_tuple->length);
    layer_mark_dirty(qr_layer);
    //layer_set_hidden(text_layer_get_layer(description_layer), true);
  }
  Tuple *descr_tuple = dict_find(iter, QUOTE_KEY_DESCRIPTION);
  if(descr_tuple)
  {
    if(descr_tuple->length<2)
      is_empty[id]=true;
    else
      is_empty[id]=false;
    //show_description(descr_tuple->value->cstring);
  }
  /*Tuple *id_tuple = dict_find(iter,QUOTE_KEY_ID);
  if(id_tuple)
  {
    id = id_tuple->value->int32+1;
  }*/
}

static void in_dropped_handler(AppMessageResult reason, void *context) {
  APP_LOG(APP_LOG_LEVEL_DEBUG, "App Message Dropped!");
}


static void app_message_init(void) {
  // Register message handlers
  app_message_register_inbox_received(in_received_handler);
  app_message_register_inbox_dropped(in_dropped_handler);
  // Init buffers
  app_message_open(app_message_inbox_size_maximum(), 100);
}

static int my_sqrt(int value)
{
  int answer = value;
  int answer_sqr = value*value;
  while(answer_sqr>value)
  {
    answer = (answer + value/answer)/2;
    answer_sqr = answer * answer;
  }
  return answer;
}

static void qr_layer_draw(Layer *layer, GContext *ctx)
{
  GRect bounds = layer_get_bounds(layer);
  if(!connected)
  {
    // Image is 80 by 80, so we have to center it
   /* GPoint origin;
    origin.x=(bounds.size.w-80)/2;
    origin.y=(bounds.size.h-80)/2;*/
    //graphics_draw_bitmap_in_rect(ctx, bluetooth_bitmap, layer_get_bounds(layer));
  }
  else if(qr_tuple){
    if(is_empty[id])
    {
      id++;
      //send_to_phone_multi(QUOTE_KEY_FETCH,id+1);
      if(!app_timer_reschedule(query_timer,100))
        query_timer=app_timer_register(100,send_query,NULL);
    }
    else
    {
      int code_length = my_sqrt(qr_tuple->length);
      //APP_LOG(APP_LOG_LEVEL_DEBUG,"Code Length: %d",code_length);
      int pixel_size = bounds.size.w/code_length;
      int width = pixel_size*code_length;
      int offset_x = (bounds.size.w-width)/2;
      //APP_LOG(APP_LOG_LEVEL_DEBUG,"Pixel Size: %d",pixel_size);
      GRect current_pixel = GRect(0, 0, pixel_size,pixel_size);
      graphics_context_set_fill_color(ctx, GColorBlack);
      for (int i=0;i<code_length;i++)
      {
        for (int j=0;j<code_length;j++)
        {
          if(qr_tuple->value->cstring[i*code_length+j]==1)
          {
            current_pixel.origin.x = j*pixel_size+offset_x;
            current_pixel.origin.y = i*pixel_size+offset_x;
            graphics_fill_rect(ctx, current_pixel,0,GCornerNone);
          }
        }
      }
    }
  }
}

static void handle_bluetooth(bool connection)
{
  if(!connection)
  {
    vibes_double_pulse();
    //show_description("No phone connection");
  }
  else
  {
    //show_description("Press Up/Down to refresh");
  }
  connected=connection;
  layer_mark_dirty(qr_layer);
}

static void window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);
#ifdef PBL_ROUND
  int offset = 28;
#else
  int offset = 4;
#endif
  bounds.origin.x = offset;
  bounds.origin.y = offset;
  bounds.size.h -= offset*2;
  bounds.size.w -= offset*2;
  qr_layer = layer_create(bounds);
  layer_add_child(window_layer, qr_layer);
  layer_set_update_proc(qr_layer, qr_layer_draw);
  
  layer_set_clips(window_layer,false);
  description_layer = text_layer_create(GRect(0, TEXT_OFFSET_Y, 5*SCREEN_WIDTH, 24));
  layer_set_clips(text_layer_get_layer(description_layer),false);
  //text_layer_set_text_alignment(description_layer,GTextAlignmentCenter);
  text_layer_set_font(description_layer, fonts_get_system_font(FONT_KEY_GOTHIC_18));
  text_layer_set_background_color(description_layer,GColorBlack);
  text_layer_set_text_color(description_layer,GColorWhite);
  text_layer_set_text(description_layer,/*description_text*/ "Comfort-Meister");
 // show_description("Loading...");
  layer_add_child(window_layer, text_layer_get_layer(description_layer));
  
  // Handle bluetooth connection
//  disconnect_image = gdraw_command_image_create_with_resource(RESOURCE_ID_DISCONNECTED_IMAGE);
  bluetooth_connection_service_subscribe(handle_bluetooth);
  handle_bluetooth(bluetooth_connection_service_peek());
  
  
  //send_to_phone_multi(QUOTE_KEY_FETCH,id+1);
}

static void window_unload(Window *window) {
//  layer_destroy(qr_layer);
//  layer_destroy(text_layer_get_layer(description_layer));
  bluetooth_connection_service_unsubscribe();
  light_enable(false);
}


static void init(void) {

  app_message_init();

  window = window_create();

  //window_set_fullscreen(window, true);
  window_set_window_handlers(window, (WindowHandlers) {
    .load = window_load,
    .unload = window_unload,
  });
  
  if(persist_exists(LAST_ID_KEY))
    id = persist_read_int(LAST_ID_KEY);
  else
    id = 4;
  const bool animated = true;
  window_stack_push(window, animated);
}

static void deinit(void) {
  window_destroy(window);
  persist_write_int(LAST_ID_KEY,id);
}

int main(void) {
  init();

  //APP_LOG(APP_LOG_LEVEL_DEBUG, "Done initializing, pushed window: %p", window);

  app_event_loop();
  deinit();
}
