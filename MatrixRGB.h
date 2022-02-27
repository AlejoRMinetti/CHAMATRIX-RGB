#ifndef MATRIXRGB_H
#define MATRIXRGB_H

class MatrixRGB
{
private:
    char brillo;
    bool MatrixON = true;
    int segs = 11, mins = 77, horas = 99;

public:
    MatrixRGB();

    void begin();

    void apagar() { MatrixON = false; }
    void encender() { MatrixON = true; }
    void onOff() { MatrixON ? apagar() : encender(); }
    bool isON() { return MatrixON; }

    void setTime(int s, int m, int h)
    {
        segs = s;
        mins = m;
        horas = h;
    }

    //brilloControl
    void brilloControl();

    ////////////// Fondos animados MATRIX RGB
    void fondoUpdate();
    //////////// sobreescribir sobre fondo
    // clock binario
    void mostrarClock();
    // set color al clock
    void setColorClock(int red, int green, int blue);
    //refresh cambios
    void refresh();
};

#endif //end lib MATRIXRGB_H