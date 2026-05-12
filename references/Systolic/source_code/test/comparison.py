import numpy as np
import matplotlib.pyplot as plt
from scipy.signal import convolve2d

IMG_SIZE = 16
SV_OUTPUT_SIZE = 14
INPUT_PATH = r"D:\output.txt.txt"

def generate_pattern(pattern_type):
    img = np.zeros((IMG_SIZE, IMG_SIZE), dtype=np.uint8)

    if pattern_type == 2:  # 横向条纹
        for i in range(IMG_SIZE):
            val = 127 if (i // 3) % 2 == 1 else 0
            img[i, :] = val
    elif pattern_type == 3:  # 纵向条纹
        for j in range(IMG_SIZE):
            val = 127 if (j // 3) % 2 == 1 else 0
            img[:, j] = val
    elif pattern_type == 4:  # 中心块状区域
        pattern_flat = [0,0,0,0,0,0,127,127,127,127,0,0,0,0,0,0] * 16
        img = np.array(pattern_flat, dtype=np.uint8).reshape((IMG_SIZE, IMG_SIZE))
    else:
        raise ValueError("Only pattern_type 2, 3, 4 are supported")
    return img

def load_sv_multiple_outputs(filepath, output_size):
    with open(filepath, 'r') as f:
        lines = f.readlines()
    blocks = []
    current_block = []
    for line in lines:
        line = line.strip()
        if not line or set(line) == {'/'}:
            if current_block and len(current_block) == output_size * output_size:
                array = np.array(current_block, dtype=np.uint16).astype(np.int16)
                blocks.append(array.reshape((output_size, output_size)))
            current_block = []
        else:
            values = [int(x) for x in line.split()]
            current_block.extend(values)
    if current_block and len(current_block) == output_size * output_size:
        array = np.array(current_block, dtype=np.uint16).astype(np.int16)
        blocks.append(array.reshape((output_size, output_size)))
    return blocks

def apply_sobel_fixedpoint(img, direction='x'):
    if direction == 'x':
        kernel = np.array([[-1, 0, 1], [-2, 0, 2], [-1, 0, 1]], dtype=np.int16)
    elif direction == 'y':
        kernel = np.array([[-1, -2, -1], [0, 0, 0], [1, 2, 1]], dtype=np.int16)
    else:
        raise ValueError("Direction must be 'x' or 'y'")
    img_int = img.astype(np.int16)
    result = convolve2d(img_int, kernel, mode='valid', boundary='symm')
    result = np.clip(result, -32768, 32767).astype(np.int16)
    return result

def normalize_to_uint8(img, use_abs=True):
    if use_abs:
        img = np.abs(img)
    img_norm = (img - img.min()) / np.ptp(img) * 255
    return np.clip(img_norm, 0, 255).astype(np.uint8)

def compare_results(sv, py):
    diff = sv.astype(np.int32) - py.astype(np.int32)
    abs_diff = np.abs(diff)
    return abs_diff

def visualize_all_comparisons(original_images, sv_conv_images, error_maps, pattern_ids):
    num_patterns = len(original_images)
    fig, axs = plt.subplots(num_patterns, 3, figsize=(12, 3 * num_patterns))
    for i in range(num_patterns):
        img = np.pad(original_images[i], pad_width=1, mode='constant', constant_values=0)
        conv = np.pad(sv_conv_images[i], pad_width=1, mode='constant', constant_values=0)
        err = np.pad(error_maps[i], pad_width=1, mode='constant', constant_values=0)

        axs[i][0].imshow(img, cmap='gray', vmin=0, vmax=127)
        axs[i][0].set_title(f'Pattern {pattern_ids[i]} - Original')
        axs[i][0].axis('off')

        axs[i][1].imshow(conv, cmap='gray', vmin=0, vmax=255)
        axs[i][1].set_title(f'Pattern {pattern_ids[i]} - SV Convolution')
        axs[i][1].axis('off')

        axs[i][2].imshow(err, cmap='hot')
        axs[i][2].set_title(f'Pattern {pattern_ids[i]} - Error Map')
        axs[i][2].axis('off')

    plt.tight_layout()
    plt.savefig('compare_all_patterns.png')
    plt.show()

if __name__ == '__main__':
    sv_outputs = load_sv_multiple_outputs(INPUT_PATH, output_size=SV_OUTPUT_SIZE)

    pattern_ids = [1, 2, 3]
    pattern_types = [2, 3, 4]
    original_images = []
    sv_conv_images = []
    error_maps = []

    for idx in range(len(pattern_types)):
        img = generate_pattern(pattern_types[idx])
        direction = 'y' if pattern_types[idx] == 2 else 'x'
        py_conv = apply_sobel_fixedpoint(img, direction=direction)
        sv_raw = sv_outputs[idx]


        sv_img = normalize_to_uint8(sv_raw, use_abs=True)

        original_images.append(img)
        sv_conv_images.append(sv_img)


    visualize_all_comparisons(original_images, sv_conv_images, error_maps, pattern_ids)
