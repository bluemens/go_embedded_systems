import csv
import pygame
import numpy as np
import struct

# level name of csv
LEVEL_NAME = "level1"

 # Offset from the left of the screen
x_offset = 120
y_offset = 50


# Load Images into Pygame
def load_image(path, scale = 2):
    image_tiles = []
    image_tiles.append(pygame.image.load("./assets/" + path + "_ul.png").convert_alpha())
    image_tiles.append(pygame.image.load("./assets/" + path + "_ur.png").convert_alpha())
    image_tiles.append(pygame.image.load("./assets/" + path + "_bl.png").convert_alpha())
    image_tiles.append(pygame.image.load("./assets/" + path + "_br.png").convert_alpha())

    for i in range(len(image_tiles)):
        image = image_tiles[i]
        image = pygame.transform.scale(image, (image.get_width() * scale, image.get_height() * scale))
        image_tiles[i] = image
    return image_tiles

# load the marble sprite
def load_sprite(path, scale = 2):
    marble = pygame.image.load("./assets/" + path).convert_alpha()
    marble = pygame.transform.scale(marble, (marble.get_width() * scale, marble.get_height() * scale))

    return marble

# Read the CSV and return a level matrix
def read_csv_to_matrix(file_path):
    matrix = []
    with open(file_path, newline='') as csvfile:
        reader = csv.reader(csvfile)
        for row in reader:
            matrix.append(row)  # Each row is a list of strings
    return matrix

# Draw a game tile to the screen with each of its respective tiles
def draw_image(screen, image_tiles, position):
    x, y = position
    width = image_tiles[0].get_width()
    for i in range(2):
        for j in range(2):
            screen.blit(image_tiles[i*2 + j], (x + j * width, y + i * width))

# Load the tiles from the folder
def load_tiles(scale):
    block = load_image("tile2", scale)
    ramp_south_west = load_image("tile3", scale)
    ramp_south_east = load_image("tile4", scale)
    pillar = load_image("tile5", scale)
    ramp_north_east = load_image("tile6", scale)
    ramp_north_west = load_image("tile7", scale)
    start = load_image("tile1", scale)
    end = load_image("tile8", scale)


    return [start, block, ramp_south_west, ramp_south_east, pillar, ramp_north_east, ramp_north_west, end], pillar

# Load the tiles from the folder
def load_marble_surface(scale):
    marble_surface = load_sprite("ball_8.png", scale)
    return marble_surface

# Write palette to file
'''
Header -
Byte 1 - Number of 16 color palettes
Groups of 4 byte RGBA colors
'''
def write_palettes(palettes, path):
    with open(path, 'wb') as f:
        f.write(struct.pack('B', len(palettes))) 
        for palette in palettes:
            write_palette(f, palette)

def write_palette(f, palette):
    counter = 0
    for r, g, b in palette:
        f.write(struct.pack('BBBB', r, g, b, 0))  # 4 bytes per color
        counter += 1
    while counter < 16:
        f.write(struct.pack('BBBB', 0, 0, 0, 0))  # 4 bytes per color
        counter += 1

# Write tilemap to file
'''
Header-
Byte 1 - Height of tilemap
Byte 2 - Width of tilemap

Rest is writing 1 byte per tile index thats an index into the unique_tiles set 
'''
def write_tilemap(tilemap, path):
    with open(path, 'wb') as f:
        height = len(tilemap)
        width = len(tilemap[0]) if height > 0 else 0
        f.write(struct.pack('B', height)) 
        f.write(struct.pack('B', width)) 
        for row in tilemap:
            for item in row:
                f.write(struct.pack('B', item)) 

# Write tiles to file
'''
Header - 
Byte 1 - Number of 8x8 tiles

The rest of file is chunks of 64 byte chunks 8x8 tiles where each value is an index into the color palette

'''
def write_texture(textures, path):
    with open(path, 'wb') as f:
        tiles_len = len(textures)
        f.write(struct.pack('B', tiles_len))  # header: number of tiles
        for texture in textures:
            for row in texture:
                for item in row:
                    f.write(struct.pack('B', item))  # write each palette index as 1 byte

# convert a surface to a np array of pixels
def surface_to_pixel_matrix(surface):
    surface = surface.convert(24)
    pixel_array = pygame.surfarray.array3d(surface)
    return np.transpose(pixel_array, (1, 0, 2)) 

# hash the np arrays
def hash_array(arr):
    return hash(arr.tobytes())

# take in array of pixels (tile) and change it to reference colors in a palette
def quantize_pixels(pixel_array, palette):
    height, width, _ = pixel_array.shape
    index_array = np.zeros((height, width), dtype=np.uint8)

    for y in range(height):
        for x in range(width):
            color = tuple(pixel_array[y, x])
            if color not in palette:
                palette.append(color)
            index_array[y, x] = palette.index(color)

    return index_array, palette

# tile mapify
def tile_mapify(screen_surface, tile_width, tile_height, textures):
    rows = screen_surface.get_height() // tile_height
    cols = screen_surface.get_width() // tile_width
    
    tilemap = []
    hash_to_index = {}
    palette = []

    for y in range(rows):
        row = []
        for x in range(cols):
            # gets a 8x8 subsurface in the screen
            tile_surface = screen_surface.subsurface(
                (x * tile_width, y * tile_height, tile_width, tile_height)
            )

            # converts surface to numpy array
            pixels = surface_to_pixel_matrix(tile_surface)
            pixels, palette = quantize_pixels(pixels, palette)

            # hash of the pixels array
            h = hash_array(pixels)

            # if the tile is unique add it to the set
            if h not in textures:
                index = len(textures)
                textures[h] = pixels.copy()
                hash_to_index[h] = index
            
            # add index to
            row.append(hash_to_index[h])
        # add the row to the tile map
        tilemap.append(row)

    # return the tile map and list of unique tiles
    return tilemap, textures, palette

# same thing as tile_mapify but slightly different - last minute addition
def marble_mapify(screen_surface, textures):
    rows = screen_surface.get_height() // 8
    cols = screen_surface.get_width() // 8
    
    tilemap = []
    hash_to_index = {}
    palette = []

    for y in range(rows):
        row = []
        for x in range(cols):
            # gets a 8x8 subsurface in the screen
            tile_surface = screen_surface.subsurface(
                (x * 8, y * 8, 8, 8)
            )

            # converts surface to numpy array
            pixels = surface_to_pixel_matrix(tile_surface)
            pixels, palette = quantize_pixels(pixels, palette)

            print(pixels)

            # hash of the pixels array
            h = hash_array(pixels)

            # if the tile is unique add it to the set
            if h not in textures:
                index = len(textures)
                textures[h] = pixels.copy()
                hash_to_index[h] = index
            
            # add index to
            row.append(hash_to_index[h])
        # add the row to the tile map
        tilemap.append(row)

    # return the tile map and list of unique tiles
    return tilemap, textures, palette


def main():
    # Pygame stuff
    pygame.init()
    screen = pygame.display.set_mode((320, 240*2))
    clock = pygame.time.Clock()
    running = True

    # Load the matrix and tiles used for rendering
    matrix = read_csv_to_matrix("./levels/" + LEVEL_NAME + ".csv")
    tiles, pillar = load_tiles(1)
    marble_surface = load_marble_surface(1)

    # Width and height of each tile to use for math
    tile_width = tiles[0][0].get_width() * 2
    tile_height = tiles[0][0].get_height() * 2 // 2

    one_iter = False
    # Game Loop 
    while(running and not one_iter):
        for event in pygame.event.get():
            if event.type == pygame.QUIT:
                running = False
        
        # Fill the background
        screen.fill("black")

        rows = len(matrix)
        cols = len(matrix[0])

        # Render loop diagonally 
        for diagonal in range(rows+cols):
            for i in reversed(range(rows)):
                j = diagonal - i

                # Check the i and j are within the bounds of the matrix
                if i >= len(matrix) or j >= len(matrix[i]) or j < 0:
                    continue

                # Each Item is a pair with the z coordinate and the tile number
                pair = matrix[i][j].split(",")
                if len(pair) > 1:
                    z = int(pair[0])
                    tile_num = int(pair[1])

                    # if the tile num is any of these, we can skip rendering it
                    if tile_num == 0:
                        continue

                    # Math for transforming the i and j in the matrix to isometric screen coordinates
                    screen_x = (2 * j - 2 * i) * (tile_width // 4) + x_offset
                    screen_y = (i + j + 2 * z) * (tile_height // 2) + y_offset

                    # Draw the columns below the tile we are drawing first from bottom up
                    number_to_draw = (480-screen_y) // (tile_height)
                    for k in reversed(range(number_to_draw)):
                        if k == 0:
                            break
                        pillar_x = screen_x
                        pillar_y = screen_y+k*tile_height
                        draw_image(screen, pillar, (pillar_x, pillar_y))
                    
                    # Drawing the tile
                    draw_image(screen, tiles[tile_num-1], (screen_x, screen_y))
        
        # Draw to the screen and time 60 fps
        textures = {}
        tilemap, textures, palette = tile_mapify(screen, 8, 8, textures)
        marble_map, textures, marble_palette = marble_mapify(marble_surface, textures)

        # list the final textures
        textures = list(textures.values())

        # write everything to binary files
        write_texture(textures, LEVEL_NAME+"-textures.bin")
        write_palettes([palette, marble_palette], LEVEL_NAME+"-palette.bin")
        write_tilemap(tilemap, LEVEL_NAME+"-tilemap.bin")
        write_tilemap(marble_map, LEVEL_NAME+"-sprites.bin")

        # only one loop iter
        one_iter = True

        pygame.display.flip()
        clock.tick(60)

    # Clean up
    pygame.quit()

if __name__ == "__main__":
    main()