#include <cekeikon.h>
#include <vector>
#include <string>
#include <iomanip>
#include <sstream>
using namespace std;

class Coin {
    public:
        Point center;
        int radius;
        double correlation;
        double radiusNorm;
        float value;

        Coin(Point center_, int radius_, double correlation_){
            this->center = center_;
            this->radius = radius_;
            this->correlation = correlation_;
            this->value = 0.0;
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

Mat_<FLT> generateCircle(int size){
    // calculando dimensao do template
    if (size % 2 == 0) size--;

    // criando o template de fundo cinza
    Mat_<FLT> circleTemplate(size, size, 128.0 / 255);
    
    // criando o circulo
    double center = size / 2 + .5;
    //contorno branco
    circle(circleTemplate, Point(center, center), center, 1.0, 1);
    // preenchimento preto
    circle(circleTemplate, Point(center, center), center - 1, 0.0, -1);
    // tratando modelo para cancelar nivel dc e soma absoluta 2
    circleTemplate = trataModelo(circleTemplate, 128.0 / 255.0);

    return circleTemplate;
}

static double Max(double a, double b) {
    return a >= b ? a : b;
}

vector<Coin> coinClassifier(vector<Coin> coins, Mat_<COR> coinsPicture){
    int maxRadius = 0;
    for(vector<Coin>::iterator coin = coins.begin(); coin != coins.end(); ++coin){
        if(coin->radius > maxRadius){
            maxRadius = coin->radius;
        }
    }
    float meanBluePercentage = 0.0;
    int meanCount = 0;
    for(vector<Coin>::iterator coin = coins.begin(); coin != coins.end(); ++coin){
        coin->radiusNorm = (float)coin->radius / (float)maxRadius;
    }
    
    for(vector<Coin>::iterator coin = coins.begin(); coin != coins.end(); ++coin){
        if(coin->radiusNorm >= .975){
            coin->value = 1.0;
        }
        else if(coin->radiusNorm >= .9){
            coin->value = .25;
        }
        else if(coin->radiusNorm < .8){
            coin->value = .1;
        }
        else{
            // moedas de 0.5 e 0.05 tem raio muito proximo
            // separar pelo nivel de magenta
            Rect rec(coin->center.x - coin->radius,
                     coin->center.y - coin->radius,
                     2 * coin->radius,
                     2 * coin->radius);
            Mat_<COR> roi = coinsPicture(rec);
            Scalar meanLevels = mean(roi);

            double dr = (double)meanLevels[2] / 255;
            double dg = (double)meanLevels[1] / 255;
            double db = (double)meanLevels[0] / 255;
            double k = 1 - Max(Max(dr, dg), db);
            double c = (1 - dr - k) / (1 - k);
            double m = (1 - dg - k) / (1 - k);
            double y = (1 - db - k) / (1 - k);
            //cout << "c: " << c << "\tm: " << m << "\ty: " << y << "\tk: " << k << endl;
            if(m < .07){
                coin->value = .5;
            }
            else{
                coin->value = .05;
            }
        }
    }
    
    return coins;
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
    
    //GaussianBlur(maskImage, maskImage, Size(5,5), 0);
    medianBlur(maskImage, maskImage, 3);
    //mostra(maskImage);

    Mat_<FLT> circleTemplate;
    Mat_<FLT> match;
    vector<Coin> coins;
    int count=0;
    for(int templateSize = .1 * maskImage.rows; templateSize < .2*maskImage.rows; templateSize++){
        if (templateSize % 2 == 0) continue;
        count++;
        // Gerando o template --------------------------------------------
        circleTemplate = generateCircle(templateSize);
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
                    //template tem borda branca
                    int radius = templateSize / 2 - 1;
                    Coin coin(Point(j, i),
                              radius,
                              match(i, j));
                    coins.push_back(coin);
                }
            }
        }
    }
    cout << count << " templates"<<endl;
    // Limpando lista de moedas -----------------------------------------
    cout << "Coins: " << coins.size() << endl;
    coins = cleanCoins(coins);
    cout << "Coins: " << coins.size() << endl;

    // classificando moedas
    coins = coinClassifier(coins, resizedImage);

    // Pintando moedas na imagem
    double ammount = 0.0;
    for(vector<Coin>::iterator coin = coins.begin(); coin != coins.end(); ++coin){
        //circle(resizedImage, coin->center, 3, Scalar(0, 0, 255), -1);
        circle(resizedImage, coin->center, coin->radius, Scalar(0, 0, 255), 2);
        
        stringstream stream;
        stream << fixed << setprecision(2) << coin->value;
        string s = stream.str();
        string coinText = string("R$") + s;
        Size textSize = getTextSize(coinText, FONT_HERSHEY_COMPLEX_SMALL, 0.5, 1, 0);
        Point textLocation = Point(coin->center.x - textSize.width / 2,
                                    coin->center.y + textSize.height / 2);
        putText(resizedImage, coinText, textLocation, 
                FONT_HERSHEY_COMPLEX_SMALL, 0.5, cvScalar(0,0,0), 1, CV_AA);

        ammount += coin->value;
    }
    // Escrevendo total de moedas e de dinheiro
    string coinsText = string("Ha ") + to_string(coins.size()) + " moedas. ";
    stringstream stream;
    stream << fixed << setprecision(2) << ammount;
    coinsText += string("Total R$ ") + stream.str() + ".";
    putText(resizedImage, coinsText, cvPoint(30,30), 
            FONT_HERSHEY_COMPLEX_SMALL, 0.8, cvScalar(0,0,255), 1, CV_AA);
    
    mostra(resizedImage);
    
    for(vector<Coin>::iterator coin = coins.begin(); coin != coins.end(); ++coin){
        cout << "(" << (coin->center.x) << ", " << (coin->center.y) << ")";
        cout << "\tcorr: " << coin->correlation;
        cout << "\tradius: " << coin->radius;
        cout << "\tnormalized radius: " << coin->radiusNorm;
        cout << "\tvalue: " << coin->value << endl;
    }

    imp(resizedImage, argv[2]);

    return 0;
}
