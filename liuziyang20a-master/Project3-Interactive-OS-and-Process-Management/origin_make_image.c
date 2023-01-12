    /* for each input file */
    for (int fidx = 0; fidx < nfiles; ++fidx) {

        int taskidx = fidx - 2;

        /* open input file */
        fp = fopen(*files, "r");
        assert(fp != NULL);

        /* read ELF header */
        read_ehdr(&ehdr, fp);
        printf("0x%04lx: %s\n", ehdr.e_entry, *files);
        /* Entry point virtual address */

        /* for each program header */
        for (int ph = 0; ph < ehdr.e_phnum; ph++) {/* Program header table entry count */

            /* read program header */
            read_phdr(&phdr, fp, ph, ehdr);

            /* write segment to the image */
            write_segment(phdr, fp, img, &phyaddr);

            /* update nbytes_kernel */
            if (strcmp(*files, "main") == 0) {
                nbytes_kernel += get_filesz(phdr);
            }
        }

        /* write padding bytes */
        /**
         * TODO:
         * 1. [p1-task3] do padding so that the kernel and every app program
         *  occupies the same number of sectors
         * 2. [p1-task4] only padding bootblock is allowed!
         */       
        printf("PHY_ADDR = %d PADDING_ADDR = %d\n",phyaddr,SECTOR_SIZE+fidx*15*SECTOR_SIZE);
        
        if (strcmp(*files, "bootblock") == 0) {
            write_padding(img, &phyaddr, SECTOR_SIZE);
        }
        else {
            write_padding(img, &phyaddr, SECTOR_SIZE+fidx*15*SECTOR_SIZE);
        }

        fclose(fp);
        files++;
    }