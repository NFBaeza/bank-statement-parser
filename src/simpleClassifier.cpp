#include "simpleClassifier.h"

namespace pdfparser {

SimpleClassifier::SimpleClassifier()
{
    initRules();
}

QString SimpleClassifier::classify(const QString &description) const
{
    for (const Rule &r : m_rules) {
        if (r.pattern.match(description).hasMatch())
            return r.category;
    }
    return QStringLiteral("others");
}

void SimpleClassifier::initRules()
{
    const auto opts = QRegularExpression::CaseInsensitiveOption;
    auto add = [this, opts](const QString &pat, const QString &cat) {
        m_rules.append({ QRegularExpression(pat, opts), cat });
    };

    // ---------- Order matters: first match wins ----------
    //
    // 1) Income / incoming money — must run BEFORE the merchant rules that
    //    might mention "transferencia" or before the generic "transferencia"
    //    rule, so "Abono por transferencia ..." classifies as a deposit.

    add(QStringLiteral("sueldo|remuneraci[oó]n|honorario|n[oó]mina|"
                       "liquidaci[oó]n\\s*sueldo|haberes"),
        QStringLiteral("paycheck"));

    add(QStringLiteral("inversi[oó]n|deposito\\s*a\\s*plazo|"
                       "fondos?\\s*mutuos?|\\bffmm\\b|cuenta\\s*2"),
        QStringLiteral("investment"));

    add(QStringLiteral("\\babono\\b|dep[oó]sito|devoluci[oó]n|reembolso|"
                       "traspaso\\s+de\\b"),
        QStringLiteral("deposit"));

    // 2) Specific merchants — Chilean retail / services. Word boundaries on
    //    short tokens (\bzara\b, \bnike\b, \btemu\b…) avoid false matches
    //    like BAEZA → ZARATE → clothes.

    add(QStringLiteral("lider|walmart|unimarc|jumbo|tottus|ekono|"
                       "(?:santa|sta)\\s*isabel|mayorista\\s*10|"
                       "esencia\\s*vegana|laicao|liquimax|"
                       "(?<!ali)\\bexpress\\b|hiper"),
        QStringLiteral("groceries"));

    add(QStringLiteral("farmacias?\\s*(?:ahumada|cruz\\s*verde|salcobrand|"
                       "knop|dr\\.?\\s*simi)|fasa|cruzverde|salcobrand|"
                       "ahumada"),
        QStringLiteral("drugstore"));

    add(QStringLiteral("uber\\s*eats|mcdonald|burger\\s*king|starbucks|"
                       "papa\\s*john|domino|dunkin|subway|\\bkfc\\b|"
                       "juan\\s*maestro|tarragona|pizza|rappi|pedidos\\s*ya|"
                       "cornershop|ifood|doggis|restauran"),
        QStringLiteral("food delivery"));

    // Gas comes BEFORE transport so copec/shell/petrobras don't get tagged
    // as transport when they're actually fuel purchases.
    add(QStringLiteral("\\bcopec\\b|\\bshell\\b|petrobras|\\benex\\b|"
                       "gasolinera|combustible|estaci[oó]n\\s*de\\s*servicio"),
        QStringLiteral("gas"));

    add(QStringLiteral("\\befe\\b|uber(?!\\s*eats)|cabify|didi|\\bbeat\\b|"
                       "c[oó]ndor|tur\\s*bus|\\bbip!?\\b|metro\\s*s\\.?a|"
                       "recarga\\s*bip"),
        QStringLiteral("transport"));

    add(QStringLiteral("enel|chilquinta|\\bcge\\b|aguas\\s*andinas|aguas\\b|"
                       "esval|essbio|entel|movistar|tel[eé]f[oó]nica|claro|"
                       "\\bwom\\b|\\bvtr\\b|mundo\\s*pac[ií]fico|\\bgtd\\b|"
                       "telsur|metrogas|gas\\s*natural|lipigas|abastible"),
        QStringLiteral("utilities"));

    add(QStringLiteral("cl[ií]nica|hospital|isapre|fonasa|colmena|consalud|"
                       "banm[eé]dica|vida\\s*tres|megasalud|integram[eé]dica|"
                       "red\\s*salud|dental|[oó]ptica|laboratorio|consulta"),
        QStringLiteral("healthcare"));

    add(QStringLiteral("universidad|colegio|instituto|escuela|"
                       "jard[ií]n\\s*infantil|\\bduoc\\b|\\binacap\\b|"
                       "\\baiep\\b|matr[ií]cula|arancel|"
                       "mensualidad\\s*escolar"),
        QStringLiteral("education"));

    add(QStringLiteral("netflix|spotify|disney|\\bhbo\\b|amazon\\s*prime|"
                       "prime\\s*video|youtube\\s*premium|\\bsteam\\b|"
                       "playstation|\\bxbox\\b|nintendo|cineplanet|cinemark|"
                       "cinepolis|hoyts|apple\\.com|google\\s*play"),
        QStringLiteral("subscription"));

    add(QStringLiteral("aliexpress|alibaba|\\btemu\\b|\\bshein\\b|"
                       "\\bwish\\b|banggood|gearbest|dhgate|\\bebay\\b"),
        QStringLiteral("online shopping"));

    add(QStringLiteral("rosen|decathlon|\\bh&?m\\b|pc\\s*factory|falabella|"
                       "ripley|paris|la\\s*polar|hites|abcdin|sodimac|"
                       "homecenter|\\beasy\\b|construmart|imperial|"
                       "\\bcorona\\b|\\bikea\\b|amazon|mercado\\s*libre|"
                       "mercadolibre|merpago|mercado\\s*pago"),
        QStringLiteral("retail"));

    add(QStringLiteral("\\bzara\\b|\\bnike\\b|\\badidas\\b|\\bpuma\\b|"
                       "\\bbata\\b|hush\\s*puppies|tricot|fashion|"
                       "forever\\s*21|flores"),
        QStringLiteral("clothes"));

    add(QStringLiteral("\\bseguro\\b|seguros|mapfre|liberty|bci\\s*seguros|"
                       "chilena\\s*consolidada|metlife|zurich|\\bhdi\\b"),
        QStringLiteral("insurance"));

    // 3) Generic money flow — runs after merchants and after deposit, so
    //    the specific-merchant signal wins over a generic "transferencia"
    //    embedded in the description.

    add(QStringLiteral("transferencia|\\btransf\\b|\\btrf\\b|\\btef\\b|"
                       "\\btraspaso\\b"),
        QStringLiteral("bank transfer"));

    add(QStringLiteral("giro.*cajero|cajero\\s*autom[aá]tico|\\batm\\b|"
                       "redbanc|retiro"),
        QStringLiteral("withdraw cash"));

    add(QStringLiteral("pago\\s*tarjeta|pago\\s*tc|pago.*cr[eé]dito|"
                       "compra\\s+nacional|compra\\s+internacional|\\bpos\\b"),
        QStringLiteral("card payment"));

    add(QStringLiteral("pago\\s*en\\s*l[ií]nea|pago.*internet|webpay|"
                       "servipag|sencillito|caja\\s*vecina|paypal|transbank"),
        QStringLiteral("online payment"));

    add(QStringLiteral("comisi[oó]n(?:es)?|amortizaci[oó]n|impuesto|"
                       "inter[eé]ses|cargo\\s*mantenci[oó]n|"
                       "costo\\s*mantenci[oó]n|mantenci[oó]n"),
        QStringLiteral("bank comission"));

    // 4) Generic purchase fallback — only fires when nothing more specific
    //    matched. Keep this LAST.
    add(QStringLiteral("\\bcompra\\b"), QStringLiteral("purchase"));
}

} // namespace pdfparser
