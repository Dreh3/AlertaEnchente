//Código desenvolvido por Andressa Sousa Fonseca

/*
*Projeto desenvolvido para funcionar como uma Estação de Alerta de Enchente
*com Simulação pelo Joystick.
*Quando certos padrões de seguranças para nível de água e volume de chuva são 
*ultrapassados, o sistema emite alertas visuais e sonoros.
*/

//Importando bilbiotecas importantes
#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "hardware/pwm.h"
#include "hardware/clocks.h"
#include "hardware/timer.h"
#include "hardware/i2c.h"
#include "lib/ssd1306.h"
#include "lib/font.h"
#include "lib/matriz.h"
#include "hardware/adc.h"

#include "FreeRTOS.h"
#include "FreeRTOSConfig.h"
#include "task.h"
#include "queue.h"

//definindo pinos do joystick
#define Eixo_X 26
#define Eixo_Y 27

//Estrutura para armazenar os dados lidos para melhor acesso
typedef struct {
    double nivel_agua;
    double volume_chuva;
}Dados;

//Criando rótulo da fila

QueueHandle_t xJoystickDados;

//Task para o Joystick, responsável por alimentar a fila
void vJoystickTask(){


    adc_init(); //Inicializa o adc

    //Inicializando entradas analógicas
    adc_gpio_init(Eixo_X); //canal 1
    adc_gpio_init(Eixo_Y); //canal 0

    Dados joystick_dados;  //Cria a variável para armazenar os dados de leitura

    Dados verificacao;

    uint contador = 0;
    uint repeticoes=0;

    //faz uma certa quantidade de leituras para verificar se os dados são consistentes antes de modificar no display
    
    while(1){

        //Salva os valores atuais de leitura para ter algo com o que comparar
        if(contador !=0){                                           //Caso o sistema esteja inicando agora, 
            verificacao.nivel_agua = joystick_dados.nivel_agua;     //não tem um dado certo para salvar.
            verificacao.volume_chuva = joystick_dados.volume_chuva;
        };

        //Faz 10 leituras num intervalo de 50ms cada para garantir que a leitura é consistente
        for(int i=0;i<10;i++){
            adc_select_input(0);
            joystick_dados.nivel_agua = (adc_read()*100)/4095;
            adc_select_input(1);
            joystick_dados.volume_chuva = (adc_read()*100)/4095;
            if(contador==0){                                            //Pega o primeiro valor para compara com as leituras seguintes
                verificacao.nivel_agua = joystick_dados.nivel_agua;
                verificacao.volume_chuva = joystick_dados.volume_chuva;
            };
            if(verificacao.nivel_agua >= joystick_dados.nivel_agua-10 && verificacao.nivel_agua <= joystick_dados.nivel_agua+10
             && verificacao.volume_chuva >= joystick_dados.volume_chuva-10 && verificacao.volume_chuva <= joystick_dados.volume_chuva+10){
                repeticoes++;   //Armzena a quantidade de vezes que o valor repetiu em 10 leituras
             };
            sleep_ms(50);
        };
        if(repeticoes >= 7){    //Se for igual 7 de 10 vezes, considera-se consistente (Pode variar para maior confiabilidade)
            xQueueSend(xJoystickDados, &joystick_dados, portMAX_DELAY); //Envia o dado para a fila
        }else{
            joystick_dados.nivel_agua = 30;     //Caso não tenha uma leitura consistente,
            joystick_dados.volume_chuva = 30;   //coloca um valor padrão
        };
        printf("Contador: %d\n", contador);     //Verificação para a quantidade repetições da task 
        printf("Volume chuva: %.2f\n", joystick_dados.volume_chuva);    
        printf("Nivel agua: %.2f\n", joystick_dados.nivel_agua);
        contador++;
        vTaskDelay(pdMS_TO_TICKS(1000));        //Pausa para evitar leituras excessivas
    };
};

//Pino do buzzer
#define Buzzer 21
#define WRAP 65535

void vBuzzerTask(){

    Dados joystick_dados;  //Cria a variável para armazenar os dados da task

    uint sons_freq[] = {880,784,740,659};   //Frequência para os sons de alerta

    //Configurações de PWM
    uint slice;
    gpio_set_function(Buzzer, GPIO_FUNC_PWM);   //Configura pino do led como pwm
    slice = pwm_gpio_to_slice_num(Buzzer);      //Adiquire o slice do pino
    pwm_config config = pwm_get_default_config();
    pwm_config_set_clkdiv(&config, clock_get_hz(clk_sys) / (4400 * WRAP));
    pwm_init(slice, &config, true);
    pwm_set_gpio_level(Buzzer, 0);              //Determina com o level desejado - Inicialmente desligado
  
  
    while(1){

        //Espia o dado na fila e verifica se foi bem sucedido
        if(xQueuePeek(xJoystickDados, &joystick_dados, portMAX_DELAY) == pdTRUE){

            //Verifica se o dado ultrapassa os limites
            if(joystick_dados.nivel_agua>=70 || joystick_dados.volume_chuva >=80){
                for(int i =0; i<4; i++){                //Laço de repetição para modificar as frequência do som de alerta
                    pwm_config_set_clkdiv(&config, clock_get_hz(clk_sys) / (sons_freq[i] * WRAP));
                    pwm_init(slice, &config, true);
                    pwm_set_gpio_level(Buzzer, WRAP/2); //Level metade do wrap para volume médio
                    vTaskDelay(pdMS_TO_TICKS(100));     //pausa entre sons
                };
                pwm_set_gpio_level(Buzzer, 0);          //Desliga o buzzer ao final do ciclo
                vTaskDelay(pdMS_TO_TICKS(100));
            }else{
                pwm_set_gpio_level(Buzzer, 0);          //Desliga o buzzer, se os dados estiverem no padrão
                vTaskDelay(pdMS_TO_TICKS(100));
            };
        }else{
            pwm_set_gpio_level(Buzzer, 0);              //Se o dado não for acessado, o buzzer é desligado
            vTaskDelay(pdMS_TO_TICKS(100));
        };
    };
};

//Configurações da matriz
PIO pio = pio0; 
uint sm = 0;    

void vMatrizLedsTask(){

    //Configurações para matriz de leds
    uint offset = pio_add_program(pio, &pio_matrix_program);
    pio_matrix_program_init(pio, sm, offset, MatrizLeds, 800000, IS_RGBW);

    //Cria a variável para armazenar os dados da task
    Dados joystick_dados;
    
    //Variável para modificar as luzes da matriz
    uint alerta=0;
    //Padrão para desligar led da matriz
    COR_RGB off = {0.0,0.0,0.0};
    //Vetor para variar entre os estados da matriz
    COR_RGB alertas[4] = {{0.0,0.0,0.0},{0.2,0.0,0.0}};

    while(true){

        //A função espia os dados na fila
        if(xQueuePeek(xJoystickDados, &joystick_dados, portMAX_DELAY) == pdTRUE){
            if(joystick_dados.nivel_agua>=70 || joystick_dados.volume_chuva >=80){
                alerta = 1; //Coloca a posição que define o led vermelho
            }else{
                alerta = 0;
            };
        }else{
            alerta = 0;
        };
        //Matriz com o padrão a ser exibido é atualizada com os valores definidos acima
        Matriz_leds alerta_visual = {{off,off,alertas[alerta],off,off},
                            {off,off,alertas[alerta],off,off},
                            {off,off,alertas[alerta],off,off},
                            {off,off,off,off,off},
                            {off,off,alertas[alerta],off,off}};
        //Exibe a matriz de leds
        acender_leds(alerta_visual);
        sleep_ms(100);
    };
};

//Configurações para comunicação I2C
#define I2C_PORT i2c1 //Comucação I2C
#define I2C_SDA 14
#define I2C_SCL 15
#define endereco 0x3C

void vDisplay3Task()
{
    // I2C Initialisation. Using it at 400Khz.
    i2c_init(I2C_PORT, 400 * 1000);

    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);                    // Set the GPIO pin function to I2C
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);                    // Set the GPIO pin function to I2C
    gpio_pull_up(I2C_SDA);                                        // Pull up the data line
    gpio_pull_up(I2C_SCL);                                        // Pull up the clock line
    ssd1306_t ssd;                                                // Inicializa a estrutura do display
    ssd1306_init(&ssd, WIDTH, HEIGHT, false, endereco, I2C_PORT); // Inicializa o display
    ssd1306_config(&ssd);                                         // Configura o display
    ssd1306_send_data(&ssd);                                      // Envia os dados para o display
    // Limpa o display. O display inicia com todos os pixels apagados.
    ssd1306_fill(&ssd, false);
    ssd1306_send_data(&ssd);

    bool cor = true;

    Dados joystick_dados;

    char nivel_agua [6];        //Strings para armazenar os dados a serem exibido no display
    char volume_chuva [6];

    while (true) //Modificar todo o design
    {
        
        //  Atualiza o conteúdo do display com as informações necessárias
        ssd1306_fill(&ssd, !cor);                          // Limpa o display
        ssd1306_rect(&ssd, 3, 1, 122, 61, cor, !cor);      // Desenha um retângulo ok
        ssd1306_line(&ssd, 3, 13, 123, 13, cor);           // Desenha uma linha ok
        ssd1306_line(&ssd, 3, 23, 123, 23, cor);           // Desenha uma linha
        ssd1306_line(&ssd, 3, 33, 123, 33, cor);           // Desenha uma linha
        ssd1306_draw_string(&ssd,"MONITORAMENTO", 12, 5); // Desenha uma string
        
        //Função extrai o dado da fila e libera para a próxima leitura
        if(xQueueReceive(xJoystickDados, &joystick_dados, portMAX_DELAY) == pdTRUE){
            
            sprintf(nivel_agua,"%.2f",joystick_dados.nivel_agua);       //passar dados para string 
            ssd1306_draw_string(&ssd,"Nv. Agua:", 4, 15);               // Desenha uma string
            ssd1306_draw_string(&ssd, nivel_agua, 78, 15);              // Desenha uma string
            sprintf(volume_chuva,"%.2f",joystick_dados.volume_chuva);   //passar dados para string 
            ssd1306_draw_string(&ssd,"V. Chuva:", 4, 25);               // Desenha uma string
            ssd1306_draw_string(&ssd, volume_chuva, 78, 25);            // Desenha uma string
            
            //Exibe mensagem de acordo com o dado
            if(joystick_dados.nivel_agua>=70 || joystick_dados.volume_chuva >=80){
                ssd1306_draw_string(&ssd,"Alerta",40,35);
                ssd1306_draw_string(&ssd,"Cuidado! Risco",5,45); 
                ssd1306_draw_string(&ssd,"de enchente.",5,55); 
            }else{
                ssd1306_draw_string(&ssd,"Estado: Normal",5,35);
                ssd1306_draw_string(&ssd,"Medidas dentro",4,45); 
                ssd1306_draw_string(&ssd,"do habitual.",5,55); 
            };
        };
        ssd1306_send_data(&ssd);                           // Atualiza o display
        sleep_ms(100);
    }
};

#define led_Green 11    //Definindo os pinos para o alerta visual
//#define led_Blue 12
#define led_Red 13

void vSemaforoRGBTask(){

    Dados joystick_dados;

    //Inicializando pinos
    gpio_init(led_Green);
    gpio_init(led_Red);
    gpio_set_dir(led_Green, GPIO_OUT);
    gpio_set_dir(led_Red, GPIO_OUT);

    gpio_put(led_Green,0);
    gpio_put(led_Red,0);

    while(true){

        //A função espia os dados na fila
        if(xQueuePeek(xJoystickDados, &joystick_dados, portMAX_DELAY) == pdTRUE){
            if(joystick_dados.nivel_agua>=70 || joystick_dados.volume_chuva >=80){
                    gpio_put(led_Green,0);
                    gpio_put(led_Red,1);        //Led vermelho para estado de alerta
            }else{
                    gpio_put(led_Green,1);      //Led verde para estado normal
                    gpio_put(led_Red,0);
            };
        }else{
                gpio_put(led_Green,1);          //Led verde para estado normal
                gpio_put(led_Red,0);
        };
        vTaskDelay(pdMS_TO_TICKS(100));
    };        

};

int main()
{
    stdio_init_all();

    //criando fila para os dados do joystick
    xJoystickDados = xQueueCreate (2,sizeof(Dados));

    //Cirando tasks
    xTaskCreate(vJoystickTask, "Task Joystick", 256, NULL, tskIDLE_PRIORITY, NULL);
    xTaskCreate(vBuzzerTask, "Task Buzzer", configMINIMAL_STACK_SIZE, NULL, tskIDLE_PRIORITY, NULL);
    xTaskCreate(vMatrizLedsTask, "Task Matriz", configMINIMAL_STACK_SIZE, NULL, tskIDLE_PRIORITY, NULL); 
    xTaskCreate(vDisplay3Task, "Task Disp3", configMINIMAL_STACK_SIZE, NULL, tskIDLE_PRIORITY, NULL);
    xTaskCreate(vSemaforoRGBTask, "Task RGB", configMINIMAL_STACK_SIZE, NULL, tskIDLE_PRIORITY, NULL);

    vTaskStartScheduler();  //Inicializa agendador
    panic_unsupported();
}
