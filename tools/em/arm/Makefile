#
# Makefile for SNSC EM ARM
#

exception_monitor-y	+= arm/exception-arm.o
exception_monitor-$(CONFIG_SNSC_EM_DISASSEMBLE)	+= arm/arm-dis.o

ccflags-y	:= -Itools/em
ccflags-y += -Wformat-security
