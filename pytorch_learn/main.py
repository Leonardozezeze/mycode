import torch
import ultralytics
print(torch.__version__)
print(torch.cuda.is_available())  # 应输出 True（如果 ROCm 配置正确）