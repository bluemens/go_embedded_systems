#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <time.h>
#include <unistd.h>

#include "vga_gpu.h"

#include "../trackball/trackball.h"
#include "game_map.h"
#include "level_reader.h"
#include "physics.h"
#include "vga_gpu.h"


/* Read the palette file and return size of the palette! */
void read_and_write_palette(const char *path, int vga_gpu_fd) {
	GPUPaletteArgs palette;

	//printf("Reading palette!\n");
	FILE *fp = fopen(path, "rb");
	if (!fp) return;

	char palette_size;
	if (!fread(&palette_size, 1, 1, fp)) return;

	//printf("Read palette size: %d\n", palette_size);

	ColorRGBA colors;
	for (int i = 0; i < palette_size; i++) {
		palette.ind = i;
		for (int j = 0; j < 16; j++) {
			if (!fread(&colors, sizeof(ColorRGBA), 1, fp))
			return;

			palette.off = j + 1;
			palette.r = colors.red;
			palette.g = colors.green;
			palette.b = colors.blue;
			palette.a = 0xFF;

			ioctl(vga_gpu_fd, VGA_GPU_WRITE_PALETTE, &palette);
		}
	}

	//printf("Read & wrote all palette colors\n");

	fclose(fp);
}

void read_and_write_textures(const char*path, int vga_gpu_fd) {
	GPUTextureArgs textures;
	Texture tex;

	//printf("Reading textures!\n");
	FILE *fp = fopen(path, "rb");
	if (!fp) return;

	char num_textures;
	if (!fread(&num_textures, 1, 1, fp)) return;

	//printf("Read %d tiles!\n", num_textures);

	
	for (int i = 0; i < num_textures; i++) {
		if (!fread(&tex, sizeof(Texture), 1, fp)) return;

		for (int j = 0; j < 8; j++) {
			for (int k = 0; k < 8; k++) {
				if(tex.palette_indicies[j][k] == 0 && i == num_textures-1) textures.palette_offs[j][k] = tex.palette_indicies[j][k];
				else textures.palette_offs[j][k] = tex.palette_indicies[j][k] + 1;
			}	
		}

		textures.ind = i;


			//printf("Texture at index i: %d\n", textures.ind);
			for(int j = 0; j < 8; j++) {
				for (int k = 0; k < 8; k++) {
					//printf("%d ", textures.palette_offs[j][k]);
				}
				//printf("\n");
			}
	
		
		

		ioctl(vga_gpu_fd, VGA_GPU_WRITE_TEXTURE, &textures);
	}

	//printf("Read & wrote all textures\n");

	fclose(fp);
}

void read_and_write_tilemap(const char *path, int vga_gpu_fd) {
	GPUTileArgs map_args;

	//printf("Reading tilemap!\n");
	FILE *fp = fopen(path, "rb");
	if (!fp) return;

	char height;
	char width;
  	if (!fread(&height, 1, 1, fp)) return;
  	if (!fread(&width, 1, 1, fp)) return;

	//printf("Reading tilemap with width: %d and height: %d\n", width, height);
	char texture_index;
	for(int y = 0; y < height; y++) {
		for(int x = 0; x < width; x++) {
			if (!fread(&texture_index, 1, 1, fp)) return;

			map_args.h_flip = 0;
			map_args.v_flip = 0;
			map_args.priority = 0;
			map_args.palette_ind = 0;
			map_args.texture_ind = texture_index;
			//map_args.texture_ind = 0;
			map_args.layer = 1;
			map_args.x = x;
			map_args.y = y;

			

			//printf("Texture Index: %d at x = %d and y = %d\n", map_args.texture_ind, map_args.x, map_args.y);

			ioctl(vga_gpu_fd, VGA_GPU_WRITE_TILE, &map_args);
		}
	}

	//printf("Finished writing tilemap\n");
	fclose(fp);
}

char read_ball_sprite(const char *path, int vga_gpu_fd) {

	//printf("Reading Sprites!\n");
	FILE *fp = fopen(path, "rb");
	if (!fp) return 0;

	char height;
	char width;
  	if (!fread(&height, 1, 1, fp)) return 0;
  	if (!fread(&width, 1, 1, fp)) return 0;

	//printf("Reading sprites with width: %d and height: %d\n", width, height);
	char texture_index;
	fread(&texture_index, 1, 1, fp);

	fclose(fp);

	//printf("Texture Index: %c", texture_index);
	return texture_index;

}

Vec2 project_3D_to_2D(Vec3 pos3D) {
	Vec2 pos2D;
	//printf("Vec3, %f, %f, %f\n", pos3D.x, pos3D.y,pos3D.z);
	pos2D.x = -(2 * pos3D.y - 2 * pos3D.x) * 4;
	pos2D.y = (pos3D.x + pos3D.y + 2 * pos3D.z) * 4;

	pos2D.x += 120.0;
	pos2D.y += 50.0;
	//printf("here\n");
	//printf("projected: %f,%f\n",pos2D.x,pos2D.y);
	return pos2D;
}

Vec3 unproject_2D_to3D(Vec2 pos2D) {
    Vec3 pos3D;

    float x = (pos2D.x - 136.0) / 2.0;
    float y = (pos2D.y - 50.0) / 4.0;

    float sum_xy = x / 2.0;
    float sum_xyz = y;

    pos3D.z = (sum_xyz - sum_xy) / 2.0;

    // Even split between x and y
    pos3D.x = sum_xy / 2.0;
    pos3D.y = sum_xy / 2.0;

    return pos3D;
}

void write_sprite_location(Vec2 state, int vga_gpu_fd, char texture_index) {
	
	GPUSpriteArgs sprite_args;

	//printf("Reading sprite!\n");
	sprite_args.x = state.x;
	sprite_args.y = state.y;
	sprite_args.ind = 0;
	sprite_args.texture_ind = texture_index;
	sprite_args.h_flip = 0;
	sprite_args.v_flip = 0;
	sprite_args.palette_ind = 1;

	ioctl(vga_gpu_fd, VGA_GPU_WRITE_SPRITE, &sprite_args);
	
	//printf("Finished writing sprite\n");
}

void clear_sprite_location(int vga_gpu_fd) {
	
	GPUSpriteArgs sprite_args;

	//printf("Reading sprite!\n");
	sprite_args.x = 0;
	sprite_args.y = 0;
	sprite_args.ind = 0;
	sprite_args.texture_ind = 0;
	sprite_args.h_flip = 0;
	sprite_args.v_flip = 0;
	sprite_args.palette_ind = 0;

	ioctl(vga_gpu_fd, VGA_GPU_WRITE_SPRITE, &sprite_args);
	
	//printf("Finished writing sprite\n");
}

void clear_all_sprites(int vga_gpu_fd) {
	GPUSpriteArgs sprite = {};
	sprite.x = 320;
	for (int i = 0; i < 256; i++)
		ioctl(vga_gpu_fd, VGA_GPU_WRITE_SPRITE, &sprite);
}



// int fd;
// void set_gpu_position(Vec2 pos) {
//   vga_gpu_pos_arg_t pos_arg;
//   pos_arg.pos.x = pos.x;
//   pos_arg.pos.y = pos.y;

//   if (ioctl(fd, VGA_gpu_WRITE_POSITION, &pos_arg) < 0) {
//     perror("ioctl");
//     close(fd);
//   }
// }

// void handle_win() { // jake help
//   printf("You Won!\n");
// }

// void handle_lose() { printf("You Lost!\n"); }

// Vec2 project_3D_to_2D(Vec3 pos3D) {
//   Vec2 pos2D;
//   pos2D.x = 2 * pos3D.x + 2 * pos3D.y;
//   pos2D.y = -pos3D.x + pos3D.y + 2 * pos3D.z;
//   return pos2D;
// }


void handle_win()
{
	printf("You Won!\n");
}


void handle_lose()
{
	printf("Died on x: %f, y: %f, z: %f",marble_state3D.pos3D.x,marble_state3D.pos3D.y,marble_state3D.pos3D.z);
	printf("You Lost!\n");
}

int main() {
	//initialize_levels(&gpu_state3D);
	//printf("%f, %f, %f\n", gpu_state3D.pos3D.x, gpu_state3D.pos3D.y,
	//		gpu_state3D.pos3D.z);

	static const char filename[] = "/dev/vga_gpu";
	static const char palette_file[] = "./levels/level4-palette.bin";
	static const char textures_file[] = "./levels/level4-textures.bin";
	static const char tilemap_file[] = "./levels/level4-tilemap.bin";
	static const char sprite_file[] = "./levels/level4-sprites.bin";

	//static const char palette_file[] = "./levels/level1-palette.bin";
	//static const char textures_file[] = "./levels/level1-textures.bin";
	//static const char tilemap_file[] = "./levels/level1-tilemap.bin";
	//static const char sprite_file[] = "./levels/level1-sprites.bin";


	GPUCtrlArgs ctrl;

	int vga_gpu_fd;

	printf("VGA GPU userspace program started\n");

	if ((vga_gpu_fd = open(filename, O_RDWR)) == -1) {
		fprintf(stderr, "could not open %s\n", filename);
		return -1;
	}

	ctrl.force_blank = 0;
	ioctl(vga_gpu_fd, VGA_GPU_WRITE_CTRL, &ctrl);

	read_and_write_palette(palette_file, vga_gpu_fd);
	read_and_write_textures(textures_file, vga_gpu_fd);
	read_and_write_tilemap(tilemap_file, vga_gpu_fd);
	clear_all_sprites(vga_gpu_fd);
	//clear_sprite_location(vga_gpu_fd);
	char texture_index = read_ball_sprite(sprite_file, vga_gpu_fd);

	initialize_levels(&marble_state3D);
	printf("starting position: %f,%f\n",marble_state3D.pos3D.x,marble_state3D.pos3D.y);
	Vec2 initial_position_proj = project_3D_to_2D(marble_state3D.pos3D);
	Vec3 unproject = unproject_2D_to3D(initial_position_proj);
	printf("%f, %f, %f\n", unproject.x, unproject.y, unproject.z);

	
	write_sprite_location(initial_position_proj, vga_gpu_fd, texture_index);
	
	if (setupReader() == -1)
	{
		return -1;
	}

	double dt = 0;

	ctrl.force_blank = 0;
	ioctl(vga_gpu_fd, VGA_GPU_WRITE_CTRL, &ctrl);

	while(1)
	{
		//add ball read
		struct ball_input input = getAccumulatedInput();

		Vec3 impulse = {input.dx, input.dy, 0};
		apply_impulse(impulse, dt);

		Vec2 current_position_proj = project_3D_to_2D(marble_state3D.pos3D);
		write_sprite_location(current_position_proj, vga_gpu_fd, texture_index);

		int state = check_boundaries(dt);

		if (state == WON)
		{
			handle_win();
			break;
		}

		if (state == LOST)
		{
			handle_lose();
			break;
		}
		
	}

	//write_sprite_location(vga_gpu_fd);

//   if (setupReader() == -1) {
//     close(fd);
//     return -1;
//   }
//   time_t start = 0, end = 0;
//   double dt;
//   // Vec2 initial_position_proj = project_3D_to_2D(gpu_state3D.pos3D);
//   // set_gpu_position(initial_position_proj);
//   while (1) {
//     // calculte time of while loop (dt)
//     // dt = difftime(end, start);

//     // // need trackball input here to define F
//     // struct ball_input input = getAccumulatedInput();

//     // Vec3 impulse = {input.dx, input.dy, 0};
//     // apply_impulse(impulse, dt);

//     // // handle game logic here

//     // int state = check_boundaries(dt);

//     // if (state == WON) {
//     //   handle_win();
//     //   break;
//     // }

//     // if (state == LOST) {
//     //   handle_lose();
//     //   break;
//     // }

//     // convert 3D state to 2D state to send to hardware
//     // send ball state to hardware
//     // Vec2 pos2D = project_3D_to_2D(gpu_state3D.pos3D);
//     // set_gpu_position(pos2D);

//     int vsync_done = read_vsync();
//     while (!vsync_done) {
//       continue;
//     }
//   }
	printf("VGA GPU userspace program terminating\n");
	//free_levels();
	close(vga_gpu_fd);
	return EXIT_SUCCESS;
}

// int main() { 
// GPUTextureArgs texture;
// GPUTileArgs tile;
// GPUSpriteArgs sprite;
// GPUPaletteArgs palette;
// GPUCtrlArgs ctrl;

// int vga_gpu_fd;
// int i, j;

// static const char filename[] = "/dev/vga_gpu";
// static const u8 colors[][4] = {
//     {0x00, 0x00, 0x00, 0x00}, /* Black */
//     {0xff, 0x00, 0x00, 0x00}, /* Red */
//     {0x00, 0xff, 0x00, 0x00}, /* Green */
//     {0x00, 0x00, 0xff, 0x00}, /* Blue */
//     {0xff, 0xff, 0x00, 0x00}, /* Yellow */
//     {0x00, 0xff, 0xff, 0x00}, /* Cyan */
//     {0xff, 0x00, 0xff, 0x00}, /* Magenta */
//     {0x80, 0x80, 0x80, 0x00}, /* Gray */
//     {0xff, 0xff, 0xff, 0x00}  /* White */
// };
// //
// // #define NUM_COLORS 9
// // #define SCREEN_WIDTH 640
// // #define SCREEN_HEIGHT 480

// printf("VGA GPU userspace program started\n");

// if ((vga_gpu_fd = open(filename, O_RDWR)) == -1) {
//   fprintf(stderr, "could not open %s\n", filename);
//   return -1;
// }

// for (i = 0; i < 8; i++) {
//   palette.r = colors[i][0];
//   palette.g = colors[i][1];
//   palette.b = colors[i][2];
//   palette.a = colors[i][3];
//   palette.ind = 0;
//   palette.off = i;
//   ioctl(vga_gpu_fd, VGA_GPU_WRITE_PALETTE, &palette);
// }

// unsigned long long gradient = 0x1234567812345678;
// for (i = 0; i < 8; i++)
//   for (j = 0; j < 8; j++)
//     texture.palette_offs[i][j] = (gradient >> ((i + j) * 4)) & 0b1111;
// texture.ind = 1;
// ioctl(vga_gpu_fd, VGA_GPU_WRITE_TEXTURE, &texture);

// sprite.ind = 0;
// sprite.texture_ind = 1;
// sprite.palette_ind = 0;
// sprite.x = 4;
// sprite.y = 4;
// ioctl(vga_gpu_fd, VGA_GPU_WRITE_SPRITE, &sprite);

// ctrl.force_blank = 0;
// ioctl(vga_gpu_fd, VGA_GPU_WRITE_CTRL, &ctrl);

// printf("VGA GPU userspace program terminating\n");
// close(vga_gpu_fd);
// return EXIT_SUCCESS;
// }
