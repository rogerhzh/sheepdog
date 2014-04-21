#!/bin/sh

collie cluster format --copies=3
qemu-img create sheepdog:alice 1G
