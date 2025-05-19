# AlertaEnchente
Repositório destinado ao desenvolvimento da Tarefa sobre utilização de Filas em Sistemas Multitarefas da Fase da Residência em Software Embarcado - Residência Embarca Tech 37

__Responsável pelo desenvolvimento:__
Andressa Sousa Fonseca

## Componentes utilizados
1) Matriz de LEDs
2) Display OLED
3) LEDs RGB
4) Buzzer
5) Joystick

## Destalhes do projeto
Os dados lidos no joystick simulam valores para Níveis de água e Volume de Chuva. Caso o valor seja maior ou igual a 70% para o nível de água ou 80% para o volume de chuva, o sistema emite alertas visuais e sonoros sobre o estado, indicando que há perigo de enchente. A matriz de LEDs exibe um sinal de alerta, os LEDs RGB oscilam entre verde e vermelho para indicar a mudança de situação. O Buzzer serve como um aporte sonoro e o display exibe informações sobre os dados em tempo real e mensagens orientando sobre a situação atual.

## Importância de filas
As filas foram tilizadas para compartilhar os dados lidos pela joystick entre as diferentes tasks. Apenas uma das tasks alimentava a fila, e as outras tasks espiavam ou recebiam o valor. A utilização de filas cotribue para melhor confiabilidade e segurança ao compartilhar informações entre tasks, garantindo que o dado seja válido.
