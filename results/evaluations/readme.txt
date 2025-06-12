1.
ground_truth 폴더에 <gt 이미지>,
output 폴더에 <비교하고자 하는 렌더링 결과 이미지>를 넣고
python3 eval_quality.py
라는 커맨드로 실행하면
각 뷰의 psnr과 평균 psnr이
콘솔에 뜸

2. 
output폴더에 넣는 이미지 포맷은
r_#.png이고, #은 0~99

3. 
nerf dataset의 eval 뷰를 사용함