typedef struct __attribute__((packed)){
	char url[128];
	char password[128];
}HouseControlConfig;

typedef struct __attribute__((packed)){
	uint8_t id;
	char name[31];
}Room;

typedef enum __attribute__((packed)){
	DEVICE_LIGHT = 0x0,
	DEVICE_LOCK = 0x1,
	DEVICE_OTHER = 0xF
}DeviceType;

typedef struct __attribute__((packed)){
	uint8_t id;
	char name[31];
	DeviceType type;
}Device;

typedef enum{
	WINDOW_HOME,
	WINDOW_ROOM,
	WINDOW_DEVICE
}WindowType;

typedef enum{
	REQUEST_ROOMS,
	REQUEST_DEVICES,
	REQUEST_DEVICE_STATUS,
	REQUEST_DEVICE_ACTION
}RequestType;

#define HTTP_APP_ID (uint32_t)'HomE'

#define HTTP_KEY_PASSWORD 1
#define HTTP_KEY_TYPE 2
#define HTTP_KEY_OFFSET 3
#define HTTP_KEY_ID 4
#define HTTP_KEY_COUNT 1
#define HTTP_KEY_DATA 2
#define HTTP_KEY_DEVID 3
#define HTTP_KEY_MAJOR 4
#define HTTP_KEY_MINOR 5

#define HTTP_TYPE_ROOMS 0
#define HTTP_TYPE_DEVICES 1
#define HTTP_TYPE_DEVICE_STATUS 2
#define HTTP_TYPE_DEVICE_ACTION 3
