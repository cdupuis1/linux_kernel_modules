#!/bin/bash

sudo rmmod misc_example
sleep 1
sudo insmod misc_example.ko
sudo ./test