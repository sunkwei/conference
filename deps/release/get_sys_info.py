#!/usr/bin/python
# coding: utf-8
# 这个脚本每隔一秒，收集 cpu/mem/指定网卡的发送/接收占用率，然后写到注册表 CURRENT_USER\Environment 中，
# 供其他程序获取这些信息。

import time
import math
import win32pdh, win32pdhutil
import os

# 注意，必须修改为你需要检测的网卡名字，可以通过“设备管理器”查询
# 这个网卡的应该与 zonekey_mcu 使用的网卡一致。
NIC_CHK = r'Intel[R] 82579LM Gigabit Network Connection'

PROCESS_USAGE = r'\Processor(_Total)\% Processor Time'
MEMORY_PERCENT = r'\Memory\% Committed Bytes In Use'
NICS_BANDWIDTH = r'\Network Interface(*)\Current Bandwidth'

class PerfMon:
    def __init__(self):
		self.base = win32pdh.OpenQuery()
		# for memory
		self.counter_mem = win32pdh.AddCounter(self.base, MEMORY_PERCENT)
		# for cpu
		self.counter_cpu = win32pdh.AddCounter(self.base, PROCESS_USAGE)
		# for nic bandwidth
		try:
			nic_bandwidth = r'\Network Interface(' + NIC_CHK + r')\Current Bandwidth'
			self.counter_nic_bandwidth = win32pdh.AddCounter(self.base, nic_bandwidth)
			nic_recv_rate = r'\Network Interface(' + NIC_CHK + r')\Bytes Received/sec'
			self.counter_nic_recv_rate = win32pdh.AddCounter(self.base, nic_recv_rate)
			nic_send_rate = r'\Network Interface(' + NIC_CHK + r')\Bytes Sent/sec'
			self.counter_nic_send_rate = win32pdh.AddCounter(self.base, nic_send_rate)
			nic_bandwidth = win32pdh.GetFormattedCounterValue(self.counter_nic_bandwidth, win32pdh.PDH_FMT_LONG)[1]			
		except:
			str = '嗯，你需要从“设备管理器”中找到网卡的名字，并修改这个脚本的第 13 行的内容'
			print str.decode('utf-8').encode('gbk')
			exit()
		
		self.reset()

    def reset(self):
		win32pdh.CollectQueryData(self.base)
		
    def getUsage(self):
		win32pdh.CollectQueryData(self.base)
		try:
			cpu = win32pdh.GetFormattedCounterValue(self.counter_cpu, win32pdh.PDH_FMT_DOUBLE)[1]
			mem = win32pdh.GetFormattedCounterValue(self.counter_mem, win32pdh.PDH_FMT_DOUBLE)[1]
			nic_bandwidth = win32pdh.GetFormattedCounterValue(self.counter_nic_bandwidth, win32pdh.PDH_FMT_LONG)[1]
			nic_recv_rate = win32pdh.GetFormattedCounterValue(self.counter_nic_recv_rate, win32pdh.PDH_FMT_DOUBLE)[1]*8
			nic_send_rate = win32pdh.GetFormattedCounterValue(self.counter_nic_send_rate, win32pdh.PDH_FMT_DOUBLE)[1]*8
			return (str(round(cpu, 2)), str(round(mem, 2)), str(round(nic_recv_rate/nic_bandwidth, 2)), str(round(nic_send_rate/nic_bandwidth, 2)))
		except:
			return ('-0.1', '-0.1', '-0.1', '-0.1', '-0.1')
		
if __name__ == "__main__":
	print ('使用了 ' + NIC_CHK + ' 作为检测用的网卡').decode('utf-8').encode('gbk')
	h = PerfMon()
	while True:
		stat = h.getUsage()
		os.putenv('zonekey_mcu_cpu_rate', stat[0])
		os.putenv('zonekey_mcu_mem_rate', stat[1])
		os.putenv('zonekey_mcu_recv_rate', stat[2])
		os.putenv('zonekey_mcu_sent_rate', stat[3])
		
		# windows 需要写入注册表
		os.system('reg add "HKEY_CURRENT_USER\\Environment" /f /v zonekey_mcu_cpu_rate /d ' + stat[0] + ' >nul')
		os.system('reg add "HKEY_CURRENT_USER\\Environment" /f /v zonekey_mcu_mem_rate /d ' + stat[1] + ' >nul')
		os.system('reg add "HKEY_CURRENT_USER\\Environment" /f /v zonekey_mcu_recv_rate /d ' + stat[2] + ' >nul')
		os.system('reg add "HKEY_CURRENT_USER\\Environment" /f /v zonekey_mcu_send_rate /d ' + stat[3] + ' >nul')
		
		print stat
		time.sleep(1)
		
