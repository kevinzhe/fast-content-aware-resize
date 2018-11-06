

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
    avg_width = (width + width - k) / 2
    cycles += (1/4)*avg_width*height/(2*8)

    # Other Seams
    # 2d conv
    cycles += 2*(9*8*height + 5*8*height)/8
    # pathsum
    cycles += 0.5*2*avg_width*height/16
    # minpath
    cycles += 2*height
    # data movement
    cycles += (1/4)*avg_width*height/(2*8)
    return cycles
