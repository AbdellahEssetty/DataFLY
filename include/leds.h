const int led_file = 25;
const int led_file_mcp = 26;
const int led_wifi = 2;

void initLeds()
{
    gpio_set_direction(led_file, GPIO_MODE_OUTPUT);
    gpio_set_direction(led_file_mcp, GPIO_MODE_OUTPUT);
    gpio_set_direction(led_wifi, GPIO_MODE_OUTPUT);
}
