/*
 *
 * File:            mandelbrot.cpp
 * Author:          Dany Shaanan
 * Website:         http://danyshaanan.com
 * File location:   https://github.com/danyshaanan/mandelbrot/blob/master/cpp/mandelbrot.cpp
 *
 * Created somewhen between 1999 and 2002
 * Rewritten August 2013
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <sys/time.h>
#include <mpi.h>

////////////////////////////////////////////////////////////////////////////////

const int MAX_WIDTH_HEIGHT = 28000;
const int HUE_PER_ITERATION = 5;
const bool DRAW_ON_KEY = true;

const int TAG_DOWORK = 0, TAG_TERMINATE = 1;

////////////////////////////////////////////////////////////////////////////////

class State {
    public:
        double centerX;
        double centerY;
        double zoom;
        int maxIterations;
        int w;
        int h;
        State() {
            //centerX = -.75;
            //centerY = 0;
            centerX = -1.186340599860225;
            centerY = -0.303652988644423;
            zoom = 350;
            maxIterations = 100;
            w = 1024;
            h = 1024;
        }
};

////////////////////////////////////////////////////////////////////////////////

double getTime() {
    struct timeval tp;
    gettimeofday(&tp, NULL);
    return ((double)tp.tv_sec + (double)tp.tv_usec * 1e-6);
}

////////////////////////////////////////////////////////////////////////////////

float iterationsToEscape(double x, double y, int maxIterations) {
    double tempa;
    double a = 0;
    double b = 0;
    for (int i = 0 ; i < maxIterations ; i++) {
        tempa = a*a - b*b + x;
        b = 2*a*b + y;
        a = tempa;
        if (a*a+b*b > 64) {
            // return i; // discrete
            return i - log(sqrt(a*a+b*b))/log(8); //continuous
        }
    }
    return -1;
}

int hue2rgb(float t){
    while (t>360) {
        t -= 360;
    }
    if (t < 60) return 255.*t/60.;
    if (t < 180) return 255;
    if (t < 240) return 255. * (4. - t/60.);
    return 0;
}

void writeImage(unsigned char *img, int w, int h) {
    long long filesize = 54 + 3*(long long)w*(long long)h;
    unsigned char bmpfileheader[14] = {'B','M', 0,0,0,0, 0,0, 0,0, 54,0,0,0};
    unsigned char bmpinfoheader[40] = {40,0,0,0, 0,0,0,0, 0,0,0,0, 1,0, 24,0};
    unsigned char bmppad[3] = {0,0,0};

    bmpfileheader[ 2] = (unsigned char)(filesize    );
    bmpfileheader[ 3] = (unsigned char)(filesize>> 8);
    bmpfileheader[ 4] = (unsigned char)(filesize>>16);
    bmpfileheader[ 5] = (unsigned char)(filesize>>24);

    bmpinfoheader[ 4] = (unsigned char)(       w    );
    bmpinfoheader[ 5] = (unsigned char)(       w>> 8);
    bmpinfoheader[ 6] = (unsigned char)(       w>>16);
    bmpinfoheader[ 7] = (unsigned char)(       w>>24);
    bmpinfoheader[ 8] = (unsigned char)(       h    );
    bmpinfoheader[ 9] = (unsigned char)(       h>> 8);
    bmpinfoheader[10] = (unsigned char)(       h>>16);
    bmpinfoheader[11] = (unsigned char)(       h>>24);

    FILE *f;
    f = fopen("temp.bmp","wb");
    fwrite(bmpfileheader,1,14,f);
    fwrite(bmpinfoheader,1,40,f);
    for (int i=0; i<h; i++) {
        long long offset = ((long long)w*(h-i-1)*3);
        fwrite(img+offset,3,w,f);
        fwrite(bmppad,1,(4-(w*3)%4)%4,f);
    }
    fclose(f);
}

unsigned char *createImage(State state) {
    int w = state.w;
    int h = state.h;

    if (w > MAX_WIDTH_HEIGHT) w = MAX_WIDTH_HEIGHT;
    if (h > MAX_WIDTH_HEIGHT) h = MAX_WIDTH_HEIGHT;

    unsigned char r, g, b;
    unsigned char *img = NULL;
    if (img) free(img);
    long long size = (long long)w*(long long)h*3;
    printf("Malloc w %zu, h %zu,  %zu\n",w,h,size);
    img = (unsigned char *)malloc(size);
    printf("malloc returned %X\n",img);

    double xs[MAX_WIDTH_HEIGHT], ys[MAX_WIDTH_HEIGHT];
    for (int px=0; px<w; px++) {
        xs[px] = (px - w/2)/state.zoom + state.centerX;
    }
    for (int py=0; py<h; py++) {
        ys[py] = (py - h/2)/state.zoom + state.centerY;
    }

    for (int px=0; px<w; px++) {
        for (int py=0; py<h; py++) {
            r = g = b = 0;
            float iterations = iterationsToEscape(xs[px], ys[py], state.maxIterations);
            if (iterations != -1) {
                float h = HUE_PER_ITERATION * iterations;
                r = hue2rgb(h + 120);
                g = hue2rgb(h);
                b = hue2rgb(h + 240);
            }
            long long loc = ((long long)px+(long long)py*(long long)w)*3;
            img[loc+2] = (unsigned char)(r);
            img[loc+1] = (unsigned char)(g);
            img[loc+0] = (unsigned char)(b);
        }
    }
    return img;
}

////////////////////////////////////////////////////////////////////////////////

void draw(State state) {
    double begin = getTime();
    unsigned char *img = createImage(state);
    double end = getTime();
    printf("time: %f\n", end - begin);

    writeImage(img, state.w, state.h);
}

void masterProcess(int n_slaves) {
    printf("I am the master with %d slaves\n", n_slaves);

    int i;
    int source;
    int flag;
    MPI_Status status;
    int num_complete = 0;
    int* start = (int*)malloc(n_slaves * sizeof(int));
    int* finish = (int*)malloc(n_slaves * sizeof(int));

    for (i = 1; i <= n_slaves; i++) {
        start[i] = i;
    }

    for (i = 1; i <= n_slaves; i++) {
        MPI_Send(&start[i], 1, MPI_INT, i, TAG_DOWORK, MPI_COMM_WORLD);
        printf("Sent %d to slave %d\n", start[i], i);
    }

    bool done = false;

    while (!done) {
        MPI_Iprobe(MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &flag, &status);
        if (flag) {
            source = status.MPI_SOURCE;

            MPI_Recv(&finish[i], 1, MPI_INT, source, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
            printf("Received %d from slave %d\n", finish[i], source);

            if (++num_complete == n_slaves) {
                done = true;
            }
        }
    }

    for (i = 1; i <= n_slaves; i++) {
        MPI_Send(NULL, 0, MPI_CHAR, i, TAG_TERMINATE, MPI_COMM_WORLD);
    }


    State state;
    draw(state);
}

void slaveProcess(int p_id) {
    printf("I am slave #%d\n", p_id);

    int num;
    MPI_Status status;
    int source = 0;
    bool done = false;

    while (!done){
        MPI_Recv(&num, 1, MPI_INT, source, MPI_ANY_TAG, MPI_COMM_WORLD, &status);

        switch (status.MPI_TAG) {
        case TAG_DOWORK:
            num <<= 1;
            MPI_Send(&num, 1, MPI_INT, source, TAG_DOWORK, MPI_COMM_WORLD);
            break;
        case TAG_TERMINATE:
            done = true;
            break;
        }
    }
}

int main(int argc, char* argv[]) {
    MPI_Status status;
    int n_proc;
    int p_id;

    MPI_Init(&argc, &argv);
    MPI_Comm_size(MPI_COMM_WORLD, &n_proc);
    MPI_Comm_rank(MPI_COMM_WORLD, &p_id);    

    switch (p_id) {
    case 0:
        masterProcess(n_proc - 1);
        break;
    default:
        slaveProcess(p_id);
        break;
    }
    MPI_Finalize();
}
















