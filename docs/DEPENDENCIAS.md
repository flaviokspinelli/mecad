# Dependências e reprodução

| Componente | Versão usada | Papel |
| --- | --- | --- |
| Qt base | 6.11.2 | Widgets, OpenGL, arquivos e testes |
| Open CASCADE | 7.9.3 | Geometria B-rep, operações e troca de arquivos |
| Apple Clang | 16 | Compilação C++20 |
| CMake | instalado no ambiente local | Configuração e build |

O núcleo geométrico e o Qt são vinculados dinamicamente. As dependências transitivas
do pacote são resolvidas pelo `macdeployqt` a partir da instalação Homebrew.

- [Qt — código e licenças](https://code.qt.io/cgit/qt/qtbase.git/tree/LICENSES?h=v6.11.2)
- [Open CASCADE — código e licenças](https://github.com/Open-Cascade-SAS/OCCT/tree/V7_9_3)

O projeto MecaCAD é destinado ao uso interno solicitado. Nenhuma licença de código
aberto foi atribuída ao código próprio. As bibliotecas de terceiros conservam
suas licenças, avisos e condições aplicáveis.

As bibliotecas de desenvolvimento foram instaladas com Homebrew neste Mac. O
aplicativo empacotado em `dist` contém as bibliotecas necessárias à execução.
O pacote é assinado localmente (ad hoc), não notarizado pela Apple.
