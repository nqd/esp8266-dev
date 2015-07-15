To read VDD
-------------------

The 107th byte in esp_init_data_default.bin (0~127byte) is named as "vdd33_const" , when TOUT pin is suspended vdd33_const must be set as 0xFF, that is 255.

Should change byte 107th of esp_init_data_default.bin to run this program.

With vi:
:%!xxd
[edit]
:%!xxd -r
:wq

To wakeup from deep sleep
------------------

Connect GPIO16 to Reset pin
