

def theoretical_peak(width, height, k):
    cycles = 0
    # Initial
    # 2d conv
    cycles += 2*(9*width*height + 5*width*height)/8
    # pathsum
    cycles += 2*width*height/16
    # minpath
    cycles += 2*height
    # data movement
    cycles += 1/4*width*height / (2*8)

    avg_width = 3*(width + width - k) / 4

    # Other Seams
    # 2d conv
    cycles += 2*k*(9*8*height + 5*8*height)/8
    # pathsum
    cycles += k*avg_width*height/16
    # minpath
    cycles += k*2*height
    # data movement
    cycles += k*(1/4)*avg_width*height/(2*8)
    return cycles

for w, h in ((352,240), (480,360), (858,480), (1280,720), (1920,1080), (3860,2160)):
  for k in range(0, w//2, w//32):
    total = theoretical_peak(w, h, k)
    print(','.join(['peak', str(w), str(h), str(k), '', '', '', '', '', '', '', str(int(total))]))

# testname  width height  num_seams_removed grey  conv  convp pathsum minpath rmpath  malloc  total size  convp_normalized pathsum_normalized  rmpath_normalized minpath_normalized  total_normalized
