#!/usr/bin/env python
# -*- coding:utf-8 -*-
import struct
import os

current_work_dir = os.path.dirname(__file__)
bt_sprd_snoop_file_dir=current_work_dir+"/hci_from_main_log.log"
bt_sprd_snoop_file=open(bt_sprd_snoop_file_dir,'r')
hci_log_filer="bt_sprd_snoop"
all_lines=bt_sprd_snoop_file.readlines()

bt_sprd_hci_log=open('bt_sprd_snoop_log.cfa','wb')
bt_sprd_hci_log.write("btsnoop\0\0\0\0\1\0\0")
s1=struct.pack('B',0x03)
s2=struct.pack('B',0xea)
bt_sprd_hci_log.write(s1)
bt_sprd_hci_log.write(s2)

for main_log_line in all_lines:
    if hci_log_filer in main_log_line:
        main_log_time,hci_ascii_str=main_log_line.split("bt_sprd_snoop_tag:   ")
        hci_ascii_str_strip=hci_ascii_str.strip().split(' ')
        for char in hci_ascii_str_strip:
            if char == '\n':
                continue
            else:
                char_2_hex=int(char, 16)
                s3=struct.pack('B',char_2_hex)
                bt_sprd_hci_log.write(s3)

bt_sprd_hci_log.close()
bt_sprd_snoop_file.close()