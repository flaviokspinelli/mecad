# Centro de giro

Em Move / Copy, o centro padrão é o centro da caixa envolvente da seleção.
Para mudar:

1. Ative **Centro de giro personalizado** e informe X/Y/Z globais; ou clique em
   **Escolher vértice para o centro de giro** e selecione um vértice CAD no desenho.
2. Ative **Girar pelo mouse**, escolha X/Y/Z e arraste o anel. A prévia é contínua.
3. Confirme com Enter/OK ou cancele com Esc. Somente escolher o pivô não altera
   a geometria nem adiciona uma operação ao histórico.

Na escolha por vértice, a geometria original substitui temporariamente a prévia.
Pode-se escolher um vértice de outro corpo CAD como referência. A rotação é feita
no pivô original e depois é aplicada a translação; o manipulador acompanha essa
translação. Desativar o pivô personalizado volta ao centro automático.

STL aceita pivô numérico ou referência em um corpo CAD, mas não seleção dos nós
da malha. Não há eixo arbitrário por dois pontos nem arraste livre do pivô nesta
implementação. A precisão dos campos é 0,001 mm.
