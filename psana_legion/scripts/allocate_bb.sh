#!/bin/bash
#SBATCH --nodes=1
#SBATCH --time=00:30:00
#SBATCH --partition=debug
#SBATCH --constraint=haswell
#SBATCH --mail-type=ALL
#SBATCH --account=lcls
#BB create_persistent name=slaughte_data_noepics capacity=1TB access=striped type=scratch
