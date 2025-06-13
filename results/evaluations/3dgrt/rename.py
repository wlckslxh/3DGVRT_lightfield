import os

for i in range(100):
    old = f"{i:05d}.png"     # 00000.png, 00001.png, ...
    new = f"r_{i}.png"       # r_0.png, r_1.png, ...
    if os.path.exists(old):
        os.rename(old, new)
        print(f"Renamed {old} → {new}")
    else:
        print(f"Skip: {old} not found")
