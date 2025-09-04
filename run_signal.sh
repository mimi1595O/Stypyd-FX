#!/bin/bash

# Activate Python virtual environment
source venv/bin/activate

# Run Python fetcher
python "datafetchv_d.py"

deactivate

# Run C++ signal processor (binary must already exist and be executable)
# Assuming binary is named 'signal' and resides in BASE_DIR/app
./signal_beta_g conf.txt

exit
