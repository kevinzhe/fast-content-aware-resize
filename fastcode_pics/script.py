import subprocess

from PIL import Image

TRIALS = 16

def bench(bin_file):
    output = subprocess.run(["ls"], stdout=subprocess.PIPE).stdout.decode('utf-8')
    for pic in output.split('\n'):

        if (pic.startswith('new') or not pic.endswith('.jpg')):
            continue

        im = Image.open(pic)
        width, height = im.size

        interval = max(width // 32, 1)

        for num_seams in range(1, width//2, interval):
            output = subprocess.run([bin_file, pic, "/dev/null", str(num_seams), str(TRIALS)], 
                stderr=subprocess.PIPE).stderr.decode('utf-8')    
            
            for trial in output.split('Running iteration'):
              times = [bin_file.split('/')[-1], str(width), str(height), str(num_seams)]
              prop_start = trial.find('grey')
              if prop_start == -1: continue
              properties = trial[prop_start:]
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
    
