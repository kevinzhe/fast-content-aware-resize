#!/bin/bash
../bin/car ../fastcode_pics/beach_1080p.jpg ./beach_1080p_out.jpg 200
compare -compose src beach_1080p_out.jpg beach_1080p_out_ref.jpg beach_1080p_out_diff.jpg
