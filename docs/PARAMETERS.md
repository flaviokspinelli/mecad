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
circulares e de arco. Polilinhas aceitam offset e fórmulas nas cotas X/Y do solver.
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

Documentos com parâmetros/vínculos usam .mcad v3, ou v4 quando há etapas
suprimidas; os demais continuam v1/v2.
Versões antigas recusam v3 em vez de descartar fórmulas silenciosamente.
Nenhum novo pacote do aplicativo é criado para mudar a versão do documento.

## Cotas de sketch

Em Sketch → Constraints, selecione uma linha ou dois vértices com Shift e use
Cota horizontal / Cota vertical. A distância é assinada (primeiro ponto menos
segundo); em uma linha, fim menos início. Informe `20 mm`, `2 cm` ou um parâmetro
como `largura / 2`. A cota e a fórmula entram como uma única etapa de undo.

As cotas aparecem no canvas do sketch e podem ser editadas clicando no valor.
Esse editor aceita a fórmula completa e também números simples em mm, inclusive
vírgula decimal. Enter aplica, Esc cancela; falha conserva o documento e mostra
o erro no editor. Remover uma restrição remove também seu vínculo, sem apagar
os parâmetros nomeados do projeto. Desvincular a fórmula mantém a medida atual.

## Limitações e próximos aceites

### Largura, altura e diâmetro no canvas

As cotas dos perfis retângulo e círculo também aceitam unidades e expressões:
por exemplo `2,5 cm` ou `largura / 2`. Números simples continuam em mm. No círculo,
o valor informado é o **diâmetro** exibido; a fórmula é convertida para o raio
armazenado. Reabrir uma cota vinculada mostra a expressão correspondente à medida
exibida. Enter sem modificar o texto não altera o documento nem acumula conversões.

Fórmulas inválidas, unidades incompatíveis e medidas não positivas deixam o editor
aberto com erro, preservando o projeto. Esc cancela. Campo vazio não remove vínculo
por acidente: use Link Dimension to Expression para desvincular explicitamente.
Isso edita os parâmetros dos perfis; não adiciona restrições radiais ao solver.

### Limites restantes

Ainda não cobre cotas angulares/radiais no solver, referências entre medidas de
recursos, renomeação com atualização automática de fórmulas,
funções matemáticas, autocomplete ou gráfico visual de dependências. O editor de
extrusão não aceita sobrescrever numericamente uma distância vinculada: edite ou
remova a fórmula no comando próprio. A tabela mostra valores calculados em unidades
canônicas e bloqueia fórmulas inválidas antes de aplicar, sem alterar o projeto.
Portanto PAR-04 ainda não está concluído.

Testes: `sh scripts/check.sh parameters`, core `namedParametersDriveGeometry`,
`expressionsDriveSketchAndAngles`, UI `namedParametersEditing` e
`expressionBoundExtrusionEditIsNoOp`. UI abre janelas; os outros testes não.
