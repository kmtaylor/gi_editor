avrdude -p m48 -c wch -P /dev/ttyS0 -e -U flash:w:control_node.hex -U eeprom:w:control_node.eep
