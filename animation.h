#define camera_frame(mask, ms) { camera_bytes(0, mask), camera_bytes(1, mask), camera_bytes(2, mask), ms }

const uint32_t camera[] = {
  0x7e0ab3d4,
  0xfa87d07a,
  0xfc137e0
};

uint32_t camera_bytes(int row, uint16_t mask) {
  mask = mask & 0xFFF;
  uint32_t row_mask;
  if (row == 0) {
    row_mask = mask << 20 | mask << 8 | mask >> 4;
  } else if (row == 1) {
    row_mask = mask << 28 | mask << 16 | mask << 4 | mask >> 8;
  } else {
    row_mask = mask << 24 | mask << 12 | mask;
  }
  return camera[row] & row_mask;
}

uint32_t startup_animation[][4] = {
  camera_frame(0x800, 80),
  camera_frame(0xC00, 60),
  camera_frame(0xE00, 50),
  camera_frame(0xF00, 40),
  camera_frame(0xF00 >> 1, 30),
  camera_frame(0xF00 >> 2, 30),
  camera_frame(0xF00 >> 3, 30),
  camera_frame(0xF00 >> 4, 30),
  camera_frame(0xF00 >> 5, 30),
  camera_frame(0xF00 >> 6, 30),
  camera_frame(0xF00 >> 7, 30),
  camera_frame(0xF00 >> 8, 40),
  camera_frame(0xF00 >> 9, 50),
  camera_frame(0xF00 >> 10, 60),
  camera_frame(0xF00 >> 11, 80),
  camera_frame(0x0, 100),
  camera_frame(0xF00 >> 11, 80),
  camera_frame(0xF00 >> 10, 60),
  camera_frame(0xF00 >> 9, 50),
  camera_frame(0xF00 >> 8, 40),
  camera_frame(0xF00 >> 7, 30),
  camera_frame(0xF00 >> 6, 30),
  camera_frame(0xF00 >> 5, 30),
  camera_frame(0xF00 >> 4, 30),
  camera_frame(0xF00 >> 3, 30),
  camera_frame(0xF00 >> 2, 30),
  camera_frame(0xF00 >> 1, 30),
  camera_frame(0xF00, 40),
  camera_frame(0xE00, 50),
  camera_frame(0xC00, 60),
  camera_frame(0x800, 80),
  camera_frame(0x0, 100)
};
