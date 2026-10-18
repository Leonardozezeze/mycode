# 贪吃蛇游戏

一个使用 C 语言和 ncurses 库开发的经典贪吃蛇游戏。

## 编译

确保系统已安装 ncurses 库：

```bash
# Arch Linux
sudo pacman -S ncurses

# Ubuntu/Debian
sudo apt-get install libncurses5-dev libncursesw5-dev

# Fedora
sudo dnf install ncurses-devel
```

使用 make 编译：

```bash
make
```

或者手动编译：

```bash
gcc snake.c -o snake -lncurses
```

## 运行

```bash
./snake
```

## 游戏规则

- 使用**方向键**控制蛇的移动方向
- 蛇吃到食物（`*`）后会变长，并增加 10 分
- 撞到墙壁或自己的身体会导致游戏结束
- 按 `q` 键退出游戏

## 清理

```bash
make clean
```

## 特性

- 经典的贪吃蛇玩法
- 使用 ncurses 实现终端图形界面
- 实时分数显示
- 碰撞检测（墙壁和自身）
- 流畅的游戏体验

享受游戏！🐍
