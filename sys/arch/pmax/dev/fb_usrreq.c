/*	$NetBSD: fb_usrreq.c,v 1.16 1999/07/25 22:50:28 ad Exp $	*/

/*ARGSUSED*/
int
fbopen(dev, flag, mode, p)
	dev_t dev;
	int flag, mode;
	struct proc *p;
{
	struct fbinfo *fi;

	if (minor(dev) >= fbndevs)
	    return(ENXIO);
	    
	fi = &fbdevs[minor(dev)].fd_info;

	if (fi->fi_open)
		return (EBUSY);

	/* Save colormap for 8bpp devices */
	if (fi->fi_type.fb_depth == 8) {
		fi->fi_savedcmap = malloc(768, M_DEVBUF, M_NOWAIT);
		if (fi->fi_savedcmap == NULL) {
			printf("fbopen: no memory for cmap\n");
			return (ENOMEM);
		}
		
		fi->fi_driver->fbd_getcmap(fi, fi->fi_savedcmap, 0, 256);
	}
	
	fi->fi_open = 1;
	(*fi->fi_driver->fbd_initcmap)(fi);

	/*
	 * Set up event queue for later
	 */
	pmEventQueueInit(&fi->fi_fbu->scrInfo.qe);
	genConfigMouse();

	return (0);
}

/*ARGSUSED*/
int
fbclose(dev, flag, mode, p)
	dev_t dev;
	int flag, mode;
	struct proc *p;
{
	struct fbinfo *fi;
	struct pmax_fbtty *fbtty;

	if (minor(dev) >= fbndevs)
	    return(EBADF);
	    
	fi = &fbdevs[minor(dev)].fd_info;

	if (!fi->fi_open)
		return (EBADF);

	fbtty = fi->fi_glasstty;
	fi->fi_open = 0;

	if (fi->fi_type.fb_depth == 8) {
		fi->fi_driver->fbd_putcmap(fi, fi->fi_savedcmap, 0, 256);
		free(fi->fi_savedcmap, M_DEVBUF);
	} else
		fi->fi_driver->fbd_initcmap(fi);

	genDeconfigMouse();

	bzero((caddr_t)fi->fi_pixels, fi->fi_pixelsize);
	(*fi->fi_driver->fbd_poscursor)
		(fi, fbtty->col * 8, fbtty->row * 15);
	return (0);
}

/*ARGSUSED*/
int
fbioctl(dev, cmd, data, flag, p)
	dev_t dev;
	u_long cmd;
	caddr_t data;
	struct proc *p;
{
	struct fbinfo *fi;
	struct pmax_fbtty *fbtty;
	char cmap_buf [3];

	if (minor(dev) >= fbndevs)
	    return(EBADF);
	    
	fi = &fbdevs[minor(dev)].fd_info;
	fbtty = fi->fi_glasstty;

	switch (cmd) {

	/*
	 * Ultrix-compatible, pm/qvss-style ioctls(). Mostly
	 * so that X consortium Xservers work.
	 */
	case QIOCGINFO:
		return (fbmmap_fb(fi, dev, data, p));

	case QIOCPMSTATE:
		/*
		 * Set mouse state.
		 */
		fi->fi_fbu->scrInfo.mouse = *(pmCursor *)data;
		(*fi->fi_driver->fbd_poscursor)
			(fi, fi->fi_fbu->scrInfo.mouse.x,
			     fi->fi_fbu->scrInfo.mouse.y);
		break;

	case QIOCINIT:
		/*
		 * Initialize the screen.
		 */
		break;

	case QIOCKPCMD:
	    {
		pmKpCmd *kpCmdPtr;
		unsigned char *cp;

		kpCmdPtr = (pmKpCmd *)data;
		if (kpCmdPtr->nbytes == 0)
			kpCmdPtr->cmd |= 0x80;
		if (!fi->fi_open)
			kpCmdPtr->cmd |= 1;
		(*fbtty->KBDPutc)(fbtty->kbddev, (int)kpCmdPtr->cmd);
		cp = &kpCmdPtr->par[0];
		for (; kpCmdPtr->nbytes > 0; cp++, kpCmdPtr->nbytes--) {
			if (kpCmdPtr->nbytes == 1)
				*cp |= 0x80;
			(*fbtty->KBDPutc)(fbtty->kbddev, (int)*cp);
		}
		break;
	    }

	case QIOCADDR:
		*(PM_Info **)data = &fi->fi_fbu->scrInfo;
		break;

	case QIOWCURSOR:
		(*fi->fi_driver->fbd_loadcursor)
			(fi, (unsigned short *)data);
		break;

	case QIOWCURSORCOLOR:
		(*fi->fi_driver->fbd_cursorcolor)(fi, (unsigned int *)data);
		break;

	case QIOSETCMAP:
		cmap_buf[0] = ((ColorMap *)data)->Entry.red;
		cmap_buf[1] = ((ColorMap *)data)->Entry.green;
		cmap_buf[2] = ((ColorMap *)data)->Entry.blue;
		(*fi->fi_driver->fbd_putcmap)
			(fi,
			 cmap_buf,
			 ((ColorMap *)data)->index,  1);
		break;

	case QIOKERNLOOP:
		genConfigMouse();
		break;

	case QIOKERNUNLOOP:
		genDeconfigMouse();
		break;

	case QIOVIDEOON:
		(*fi->fi_driver->fbd_unblank) (fi);
		break;

	case QIOVIDEOOFF:
		(*fi->fi_driver->fbd_blank) (fi);
		break;


	/*
	 * Sun-style ioctls, mostly so that screenblank(1) and other
	 * ``native'' NetBSD applications work.
	 */
	case FBIOGTYPE:
		*(struct fbtype *)data = fi->fi_type;
		break;

	case FBIOGETCMAP:
		return ((*(fi->fi_driver -> fbd_getcmap))
			(fi, data, 0, fi->fi_type.fb_cmsize));

	case FBIOPUTCMAP:
		return ((*(fi->fi_driver -> fbd_putcmap))
			(fi, data, 0, fi->fi_type.fb_cmsize));
		break;

	case FBIOGVIDEO:
		*(int *)data = (fi->fi_blanked) ? FBVIDEO_OFF: FBVIDEO_ON;
		break;

	case FBIOSVIDEO:
		if (*(int *)data == FBVIDEO_OFF)
			return (*(fi->fi_driver->fbd_blank)) (fi);
		else
			return (*(fi->fi_driver->fbd_unblank)) (fi);

	default:
		printf("fb%d: Unknown ioctl command %lx\n", minor(dev), cmd);
		return (EINVAL);
	}
	return (0);
}


/*
 * Poll on Digital-OS-compatible in-kernel input-event ringbuffer.
 */
int
fbpoll(dev, events, p)
	dev_t dev;
	int events;
	struct proc *p;
{
	struct fbinfo *fi;
	int revents = 0;

	if (minor(dev) >= fbndevs)
	    return(EBADF);
	    
	fi = &fbdevs[minor(dev)].fd_info;

	if (events & (POLLIN | POLLRDNORM)) {
		if (fi->fi_fbu->scrInfo.qe.eHead !=
		    fi->fi_fbu->scrInfo.qe.eTail)
		 	revents |= (events & (POLLIN|POLLRDNORM));
		else
	  		selrecord(p, &fi->fi_selp);
	}

	/* XXX mice are not writable, what to do for poll on write? */
#ifdef notdef
	if (events & (POLLOUT | POLLWRNORM))
		revents |= events & (POLLOUT | POLLWRNORM);
#endif

	return (revents);
}


/*
 * Return the physical page number that corresponds to byte offset 'off'.
 */
/*ARGSUSED*/
int
fbmmap(dev, off, prot)
	dev_t dev;
	int off, prot;
{
	struct fbinfo *fi;
	int len;

	if (off < 0)
		return (-1);

	if (minor(dev) >= fbndevs)
	    return(-1);
	    
	fi = &fbdevs[minor(dev)].fd_info;

	len = mips_round_page(((vaddr_t)fi->fi_fbu & PGOFSET)
			      + sizeof(*fi->fi_fbu));
	if (off < len)
		return (int)mips_btop(MIPS_KSEG0_TO_PHYS(fi->fi_fbu) + off);
	off -= len;
	if (off >= fi->fi_type.fb_size)
		return (-1);
	return (int)mips_btop(MIPS_KSEG1_TO_PHYS(fi->fi_pixels) + off);
}
