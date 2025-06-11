import os
import cv2
import numpy as np

gt_dir = 'ground_truth'
test_dir = 'output'

psnr_list = []

def calculate_psnr(img1_path, img2_path):
    img1 = cv2.imread(img1_path)
    img2 = cv2.imread(img2_path)
    mse = np.mean((img1.astype(np.float32) - img2.astype(np.float32)) ** 2)
    if mse == 0:
        return float('inf')
    PIXEL_MAX = 255.0
    psnr = 10 * np.log10((PIXEL_MAX ** 2) / mse)
    return psnr

for fname in os.listdir(gt_dir):
    gt_path = os.path.join(gt_dir, fname)
    test_path = os.path.join(test_dir, fname)
    if os.path.exists(test_path):
        psnr = calculate_psnr(gt_path, test_path)
        psnr_list.append(psnr)
        print(fname, "PSNR:", psnr)

avg_psnr = sum(psnr_list) / len(psnr_list)
print("Average PSNR:", avg_psnr)
