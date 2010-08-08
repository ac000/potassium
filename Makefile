LIBS=`pkg-config --libs clutter-1.0 glib-2.0 gstreamer-0.10`
INCS=`pkg-config --cflags clutter-1.0 glib-2.0 gstreamer-0.10`
LM=`echo -I${HOME}/programming/projects/project-x/libmozart -L${HOME}/programming/projects/project-x/libmozart -lmozart`

potassium: potassium.c
	gcc -Wall potassium.c -o potassium $(LM) $(INCS) $(LIBS)

clean:
	rm potassium

