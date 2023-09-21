obj-m += vmhelper.o
all:
	make -C `realpath $(BUILD_PATH)` M=$(PWD) modules
clean:
	make -C `realpath $(BUILD_PATH)` M=$(PWD) clean