#!/bin/bash
../rippled account_objects rGGLq3bp1oMjzFwwXnt3kMVtqgKpcue957 | sed 's/CreateCode".*/CreateCode" : <truncated>/g'

