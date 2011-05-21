LIBS=`pkg-config --libs clutter-1.0 glib-2.0 gstreamer-0.10` -L../libmozart -lmozart
INCS=`pkg-config --cflags clutter-1.0 glib-2.0 gstreamer-0.10` -I../libmozart

potassium: potassium.c
	gcc -g -Wall -Wl,-rpath,../libmozart potassium.c -o potassium $(INCS) $(LIBS)

clean:
	rm potassium

