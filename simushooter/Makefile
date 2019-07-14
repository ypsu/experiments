.PHONY: clean all
space :=
space +=

all: \
	build/simushooter \
	assets/textures.pam \
	assets/world.geom \
	assets/sounds.raw \
	build/stripe_hitmap \

clean:
	rm -f build/*

platform := $(shell if test -d /opt/vc/lib; then echo rpi; else echo linux; fi)
srcs := $(wildcard code/*.c)
objs := $(patsubst code/%.c,build/%.o,$(srcs))

sdl_cflags := $(shell sdl2-config --cflags)
cflags := -Wall -Wextra -Werror -g -std=c99 $(sdl_cflags)
ldflags := -lSDL2 -lm -lopenal

ifeq ($(platform),rpi)
	cflags += -DPLATFORM_RPI
	libpaths := \
		~/proj/aur/sdl2/src_good/SDL2-2.0.3/build/.libs/ \
		/opt/vc/lib
	ldflags += \
		$(addprefix -L ,$(libpaths)) \
		-Xlinker -rpath -Xlinker $(subst $(space),:,$(libpaths)) \
		-lGLESv2
else
	ldflags += -lGL
endif

build/simushooter: $(objs)
	@echo linking $@
	@gcc -o $@ $(objs) $(ldflags)

build/%.o: code/%.c code/headers.h
	@echo compiling $<
	@gcc $(cflags) -c $< -o $@

raw_textures := $(shell ls assets/raw_textures/*.png)
raw_hitmaps := $(shell ls assets/raw_hitmaps/*.pgm)

assets/textures.desc: $(raw_textures) $(raw_hitmaps) build/gen_atlas
	@echo putting textures into $@
	@./build/gen_atlas $(raw_textures) > $@

assets/textures.pam: assets/textures.desc $(raw_textures) utils/gen_textures.sh
	./utils/gen_textures.sh

raw_sounds := $(shell ls assets/raw_sounds/*.wav)

assets/sounds.raw: $(raw_sounds) utils/gen_sounds.sh
	./utils/gen_sounds.sh

assets/world.geom: assets/world.map build/gen_geometry
	@echo constructing world geometry into $@
	@./build/gen_geometry < $< > $@

build/gen_geometry: utils/gen_geometry.c
	@echo building $@
	@gcc $(cflags) -o $@ $<

build/gen_atlas: utils/gen_atlas.c
	@echo building $@
	@gcc $(cflags) -o $@ $<

build/stripe_hitmap: utils/stripe_hitmap.c
	@echo building $@
	@gcc $(cflags) -o $@ $<
