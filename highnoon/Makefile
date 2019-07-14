.PHONY: clean all

includes += \
	-isystem /opt/vc/include \
	-isystem /opt/vc/include/interface/vcos/pthreads \
	-isystem /opt/vc/include/interface/vmcs_host/linux \

cflags := \
	-Wall -Wextra -Werror \
	-g -fgnu89-inline -std=gnu99 \
	$(includes)

libs := -lSDL -lGLESv2 -lEGL -lbcm_host -lm
libpaths := /opt/vc/lib
ldflags := \
	$(addprefix -L ,$(libpaths)) \
	-Xlinker -rpath -Xlinker $(subst $(space),:,$(libpaths)) \
	$(libs) \

all: highnoon sounds.raw textures.tga

headers := $(wildcard code/*.h)
sources := $(wildcard code/*.c)
binaries := $(patsubst %.c,%.o,$(sources))

highnoon: $(binaries)
	@echo linking $@
	@gcc $(cflags) $(binaries) -o $@ $(ldflags)

code/headers.h.gch: $(headers)
	@echo precompiling headers
	@gcc $(cflags) -c code/headers.h -o $@

%.o: %.c code/headers.h.gch
	@echo compiling $<
	@gcc $(cflags) -c $< -o $@

sounds.raw: sounds.desc $(wildcard data/*.wav)
	./gen_sounds.sh

textures.tga: textures.desc $(wildcard data/*.png)
	./gen_textures.sh

clean:
	rm -f highnoon $(binaries) code/headers.h.gch
