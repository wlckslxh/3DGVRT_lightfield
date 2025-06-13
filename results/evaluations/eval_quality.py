import os
import cv2
import numpy as np
from skimage.metrics import structural_similarity as ssim

gt_dir = 'ground_truth'
test_dirs = ['3dgvrt', 'vk3dgs', '3dgrt']

def calculate_psnr(img1, img2):
    mse = np.mean((img1.astype(np.float32) - img2.astype(np.float32)) ** 2)
    if mse == 0:
        return float('inf')
    PIXEL_MAX = 255.0
    return 10 * np.log10((PIXEL_MAX ** 2) / mse)

def calculate_ssim(img1, img2, win_size=7):
    # BGR → RGB
    img1_rgb = cv2.cvtColor(img1, cv2.COLOR_BGR2RGB)
    img2_rgb = cv2.cvtColor(img2, cv2.COLOR_BGR2RGB)

    # 이미지 크기 확인
    h, w = img1_rgb.shape[:2]
    if min(h, w) < win_size:
        # 이미지가 작으면 win_size를 줄임
        win_size = min(h, w) if min(h, w) % 2 == 1 else min(h, w) - 1

    # channel_axis=-1로 채널 위치 지정
    score = ssim(
        img1_rgb,
        img2_rgb,
        channel_axis=-1,
        win_size=win_size
    )
    return score

for td in test_dirs:
    psnr_list = []
    ssim_list = []
    print(f"\n=== {td} ===")
    for fname in sorted(os.listdir(gt_dir)):
        gt_path   = os.path.join(gt_dir,   fname)
        test_path = os.path.join(td,        fname)
        if not os.path.exists(test_path):
            print(f"  skip {fname}")
            continue

        gt_img   = cv2.imread(gt_path)
        test_img = cv2.imread(test_path)
        
        # print(gt_path, gt_img.shape) 
        # print(test_path, test_img.shape)

        p = calculate_psnr(gt_img, test_img)
        s = calculate_ssim(gt_img, test_img)

        psnr_list.append(p)
        ssim_list.append(s)
        print(f"{fname}: PSNR={p:.2f}, SSIM={s:.4f}")

    avg_psnr = sum(psnr_list) / len(psnr_list) if psnr_list else float('nan')
    avg_ssim = sum(ssim_list) / len(ssim_list) if ssim_list else float('nan')
    print(f"{td} Average → PSNR: {avg_psnr:.2f}, SSIM: {avg_ssim:.4f}")