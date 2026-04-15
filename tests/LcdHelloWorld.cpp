#include "PinConfig.h"
#include "hardware/LcdDisplay.h"

#include <iostream>
#include <thread>
#include <chrono>

int main() {
    std::cout << "Initializing LCD...\n";
    LcdDisplay lcd(LCD_I2C_BUS, LCD_I2C_ADDRESS);

    if (!lcd.init()) {
        std::cerr << "Failed to initialise LCD.\n";
        return 1;
    }

    lcd.print(0, "Hello World");
    lcd.print(1, "                ");

    std::cout << "Displaying 'Hello World' on the LCD for 10 seconds...\n";
    std::this_thread::sleep_for(std::chrono::seconds(10));

    lcd.shutdown();
    std::cout << "Done.\n";
    return 0;
}
