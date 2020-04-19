prefix=server_
target_deepin=deepin
target_arm=arm
target_mipsel=mipsel
src=$(wildcard ./*.c)
# obj=$(patsubst ./%.c,%.o,$(src))



$(target_deepin):$(src)
	gcc -g -Wall $^ -o $(prefix)$@

$(target_arm):$(src)
	arm-linux-gcc -Wall -static $^ -o $(prefix)$@

$(target_mipsel):$(src)
	mipsel-linux-gnu-gcc -Wall -static $^ -o $(prefix)$@

# all:$(src)
# 	gcc -Wall $^ -o $(prefix)$(target_deepin)
# 	arm-linux-gcc -Wall -static $^ -o $(prefix)$(target_arm) 
# 	mipsel-linux-gnu-gcc -Wall -static $^ -o $(prefix)$(target_mipsel)

# %.o:%.c
# 	mipsel-linux-gnu-gcc  -Wall  -c $< -o $@

clean:
	rm $(prefix)$(target_deepin) $(prefix)$(target_arm) $(prefix)$(target_mipsel)
