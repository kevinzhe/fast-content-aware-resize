# Fast content-aware resize Optimized Implementation

### Overview
This contains our optimized implementation of seam carving. We have included a zipped file, `base.zip` which contains our baseline implementation. `\fastcode_pics` contains pictures and a script for benchmarking purposes. 

### Build

To build the optimized implementation binary, run

`make` 

This creates binary `car` inside the `\bin` folder

The binary takes in 3 arguments - input file, output file and number of seams to remove.

For example,

`car beach_1080p.jpg new_beach_1080p.jpg 3`

We have already included the binary of our baseline, `base` inside `\bin`. To rebuild, unzip `base.zip` and follow README instructions in there. 

### Benchmarking

To run the benchmarking script, cd into `fastcode_pics` and run `python36 script.py`. The output generated is in .csv format for ease of benchmarking.

### Correctness

In `output_pics`, we provide the script `check.sh` for checking the correctness of our implementation by comparing the output picture of our `car` binary with a reference picture. 
