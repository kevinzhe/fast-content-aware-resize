import subprocess

from PIL import Image

def bench(bin_file):
    output = subprocess.run(["ls"], stdout=subprocess.PIPE).stdout.decode('utf-8')
    for pic in output.split('\n'):

        if (pic.startswith('new') or not pic.endswith('.jpg')):
            continue

        im = Image.open(pic)
        width, height = im.size

        for num_seams in range(1, width//2, 20):
            times = [bin_file.split('/')[-1], str(width), str(height), str(num_seams)]
            output = subprocess.run([bin_file, pic, "/dev/null", str(num_seams)], 
                stderr=subprocess.PIPE).stderr.decode('utf-8')    
            
            properties = output[output.find("grey"):]
            for prop in properties.split('\n'):
                if "Completed in" in prop:
                    break
                idx = prop.find("\t")
                time_str = prop[idx+1:]
                end = time_str.find("\t")
                time = time_str[:end]
                times.append(time)
            print(','.join(times))

hdr = 'testname, width, height, num_seams_removed, grey, conv, convp, pathsum, minpath, rmpath, malloc, total'
print(','.join(hdr.split(', ')))
bench("../bin/base")
bench("../bin/car")

    # output = subprocess.run(["../../go_projects/bin/caire","-in", line,"-out", 
    # "new_"+line, "-width=" + str(new_width)], 
    # stdout=subprocess.PIPE).stdout.decode('utf-8')
    # substr = "Rescaled in: "
    # idx = output.find(substr)
    # new_str = output[idx+len(substr):]
    # end_idx = new_str.find("s")
    # time = new_str[:end_idx]
    # times.append(time)
    