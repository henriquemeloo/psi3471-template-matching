#include <cekeikon.h>
#include <vector>
#include <string>
using namespace std;

class Coin {
    public:
        Point center;
        int radius;
        double correlation;

        Coin(Point center_, int radius_, double correlation_){
            center = center_;
            radius = radius_;
            correlation = correlation_;
        }
};

vector<Coin> cleanCoins(vector<Coin> coins){
    int i = 0;
    int j = 0;
    while(i < coins.size() - 1){
        bool erased = false;
        // limites da busca por outra moeda
        int boundary = coins[i].radius / 2;
        
        j = i + 1;
        while(j < coins.size()){
            if((coins[j].center.x <= coins[i].center.x + boundary) &&
               (coins[j].center.x >= coins[i].center.x - boundary) &&
               (coins[j].center.y <= coins[i].center.y + boundary) &&
               (coins[j].center.y >= coins[i].center.y - boundary)){
                // trata-se da mesma moeda, manter so a de maior correlacao
                if(coins[i].correlation > coins[j].correlation){
                    coins.erase(next(coins.begin(), j));
                }
                else{
                    coins.erase(next(coins.begin(), i));
                    erased = true;
                    break;
                }
            }
            else{
                j++;
            }
        }
        if(! erased){
            i++;
        }
    }
    return coins;
}

Mat_<FLT> generateCircle(int size, double ratio){
    // calculando dimensao do template
    int dimension = (int) (ratio * size);
    if (dimension % 2 == 0) dimension--;

    // criando o template de fundo cinza
    Mat_<FLT> circleTemplate(dimension, dimension, 128.0 / 255);
    
    // criando o circulo
    double center = dimension / 2 + .5;
    int radius = (int) center;
    //contorno branco
    circle(circleTemplate, Point(center, center), radius, 1.0, -1);
    // preenchimento preto
    circle(circleTemplate, Point(center, center), radius - 1, 0.0, -1);

    // tratando modelo para cancelar nivel dc e soma absoluta 2
    circleTemplate = trataModelo(circleTemplate, 128.0 / 255.0);

    return circleTemplate;
}


int main(int argc, char** argv){
    if (argc != 3) {
        printf("ep1 entrada.jpg saida.jpg\n");
        erro("Erro: Numero de argumentos invalido");
    }

    Mat_<COR> originalImage;
    le(originalImage, argv[1]);

    // Reduzindo imagem --------------------------------------------------
    double ratio = .2;
    Mat_<COR> resizedImage;
    resize(originalImage, resizedImage, Size(0,0), ratio, ratio, INTER_NEAREST);
    cout << "Original image: " << originalImage.rows << "x" << originalImage.cols << "\n";
    cout << "Resized image: " << resizedImage.rows << "x" << resizedImage.cols << "\n";

    // Convertendo imagem reduzida para niveis de cinza------------------
    Mat_<GRY> greyscaleImage;
    cvtColor(resizedImage, greyscaleImage, CV_BGR2GRAY);
    
    // Suavizando imagem -------------------------------------------------
    Mat_<COR> maskBuilder;
    GaussianBlur(resizedImage, maskBuilder, Size(5,5), 0);
    // Convertendo imagem suavizada para HSV -----------------------------
    cvtColor(maskBuilder, maskBuilder, CV_BGR2HSV);
    // Filtrando por niveis HSV ------------------------------------------
    Mat_<FLT> maskImage(maskBuilder.rows, maskBuilder.cols, 0.0);
    for (int i = 0; i < maskBuilder.total(); i++){
        if (maskBuilder(i)[0] > 35){
            maskImage(i) = 1.0;
        }
        else{
            maskImage(i) = 0.0;
        }
    }
    
    //GaussianBlur(maskImage, maskImage, Size(5,5), 2, 2);
    mostra(maskImage);
    //imp(maskImage, "median_13_h_35.jpg");

    Mat_<FLT> circleTemplate;
    Mat_<FLT> match;
    vector<Coin> coins;
    //set<tuple<int, int>> coinLocations;
    for(double templateRatio = .1; templateRatio <= .2; templateRatio += .0005){
        // Gerando o template --------------------------------------------
        //cout << "Ratio: " << templateRatio << endl;
        circleTemplate = generateCircle(maskImage.rows, templateRatio);
        //mostra(circleTemplate);

        // Fazendo template matching -------------------------------------
        matchTemplate(maskImage, circleTemplate, match, CV_TM_CCOEFF);
        match = aumentaCanvas(match,
                            maskImage.rows,
                            maskImage.cols,
                            (circleTemplate.rows - 1) / 2,
                            (circleTemplate.cols - 1) / 2,
                            0.0);
        //mostra(match);
        threshold(match, match, 0.6, 1.0, THRESH_TOZERO);

        // Isolando o centro do pico de correlacao -----------------------
        // O filtro gaussiano e aplicado para fazer com que nao haja 2 pixeis
        // de correlacao de mesmo valor.
        GaussianBlur(match, match, Size(5,5), 0);
        int ksize = 11;
        for(int i = ksize; i < match.rows - ksize; i++){
            for(int j = ksize; j < match.cols - ksize; j++){
                for(int k = i - ksize; k <= i + ksize; k++){
                    for(int l = j - ksize; l <= j + ksize; l++){
                        if((k == i) && (l == j)){
                            continue;
                        }
                        if(match(k, l) > match(i, j)){
                            match(i, j) = 0;
                        }
                    }
                }
            }
        }

        // Guardando moedas encontradas ----------------------------------
        for(int i = 0; i < match.rows; i++){
            for(int j = 0; j < match.cols; j++){
                if(match(i, j) > 0){
                    int radius = (int) (maskImage.rows * templateRatio / 2 + .5);
                    Coin coin(Point(j, i),
                              radius,
                              match(i, j));
                    coins.push_back(coin);
                }
            }
        }
    }
    
    // Limpando lista de moedas -----------------------------------------
    cout << "Coins: " << coins.size() << endl;
    coins = cleanCoins(coins);
    cout << "Coins: " << coins.size() << endl;

    for(vector<Coin>::iterator coinsIt = coins.begin(); coinsIt != coins.end(); ++coinsIt){
        cout << "(" << (coinsIt->center.x) << ", " << (coinsIt->center.y) << ")";
        cout << " corr: " << coinsIt->correlation;
        cout << " radius: " << coinsIt->radius << endl;
    }

    // Pintando moedas na imagem
    for(vector<Coin>::iterator coinsIt = coins.begin(); coinsIt != coins.end(); ++coinsIt){
        circle(resizedImage, coinsIt->center, 3, Scalar(0, 0, 255), -1);
        circle(resizedImage, coinsIt->center, coinsIt->radius, Scalar(0, 0, 255), 3);
    }

    // Escrevendo total de moedas
    string coinsText = string("Ha ") + to_string(coins.size()) + " moedas.";
    putText(resizedImage, coinsText, cvPoint(30,30), 
            FONT_HERSHEY_COMPLEX_SMALL, 0.8, cvScalar(0,0,255), 1, CV_AA);
    
    mostra(resizedImage);

    //imp(resizedImage, argv[2]);

    return 0;
}