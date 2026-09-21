# Parâmetros e expressões — implementação de desenvolvimento

Não altera a distribuição 0.2.25 em dist. Item PAR-04 parcial.

## Uso

No menu **MODIFY → Change Parameters**, defina nomes e fórmulas. Exemplo:

| Nome | Expressão |
| --- | --- |
| largura | 40 mm |
| parede | 2 mm |
| interno | largura - 2 * parede |

Selecione o recurso no Browser ou histórico e abra **MODIFY → Link Dimension to
Expression**. Escolha o campo e informe a fórmula. Vazio remove o vínculo e mantém
o valor resolvido. A medida vinculada é somente leitura no painel de propriedades;
o tooltip identifica a fórmula. Não é necessário editar o arquivo manualmente.

Campos atuais: dimensões e posições de primitivas, extrusão d, filete r,
chanfro d/d2/angle, revolução angle/axis, deslocamentos/pivô/ângulo de movimento
e cópia, medidas do furo e coordenadas/medidas de sketches retangulares,
circulares e de arco. Polilinhas aceitam offset; suas cotas restritas ainda não.
Campos angle exigem ângulo explícito; os demais exigem comprimento. Uma fórmula negativa só é aceita quando a
operação geométrica permite, por exemplo extrusão em sentido contrário.

## Gramática e unidades

- Nomes ASCII iniciados por letra ou sublinhado, até 64 caracteres, sem espaços.
- Operadores `+ - * /`, sinais unários e parênteses; precedência aritmética comum.
- Decimal com ponto ou vírgula, sem separadores de milhar; notação científica.
- Comprimentos: mm, cm, m, in. Ângulos: deg, rad. Constante pi.
- Exemplos angulares: `90 deg` ou `pi * rad` (multiplicação explícita após pi).
- Multiplicação explícita entre variáveis: `2 * parede`, não `2parede`.
- Conversão interna para mm/radianos; dimensões verificadas em cada operação.
- Não há funções, scripts, acesso a arquivos, atribuições ou avaliação de código.

Limites: 256 parâmetros, expressão de até 4096 caracteres, 64 níveis sintáticos,
expoentes dimensionais entre -16 e 16. Valores não finitos, nomes desconhecidos,
unidades incompatíveis, divisão por zero e dependências circulares são recusados.

## Integridade

A resolução ocorre em cópia do documento. Falha de fórmula ou geometria não
altera o estado anterior. Aceitar gera uma etapa de undo, cancelar não altera o
documento. Exclusão de parâmetro utilizado é recusada; remover vínculo é explícito.
Fórmulas são a fonte de verdade, valores numéricos são cache reconstruído.

Documentos com parâmetros/vínculos usam .mcad v3; os demais continuam v1/v2.
Versões antigas recusam v3 em vez de descartar fórmulas silenciosamente.
Nenhum novo pacote do aplicativo é criado para mudar a versão do documento.

## Limitações e próximos aceites

Ainda não cobre cotas/restrições de sketches, referências entre medidas de
recursos, renomeação com atualização automática de fórmulas,
funções matemáticas, autocomplete ou gráfico visual de dependências. O editor de
extrusão não aceita sobrescrever numericamente uma distância vinculada: edite ou
remova a fórmula no comando próprio. A tabela ainda não mostra coluna de valores
resolvidos. Portanto PAR-04 não está concluído.

Testes: `sh scripts/check.sh parameters`, core `namedParametersDriveGeometry`,
`expressionsDriveSketchAndAngles`, UI `namedParametersEditing` e
`expressionBoundExtrusionEditIsNoOp`. UI abre janelas; os outros testes não.
