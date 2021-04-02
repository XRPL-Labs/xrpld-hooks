const minMantissa = 1000000000000000n
const maxMantissa = 9999999999999999n
const minExponent = -96
const maxExponent = 80

function make_xfl(exponent, mantissa)
{
    // convert types as needed
    if (typeof(exponent) != 'bigint')
        exponent = BigInt(exponent);

    if (typeof(mantissa) != 'bigint')
        mantissa = BigInt(mantissa);

    // canonical zero
    if (mantissa == 0n)
        return 0n;

    // normalize
    let is_negative = mantissa < 0;
    if (is_negative)
        mantissa *= -1n;

    while (mantissa > maxMantissa)
    {
        mantissa /= 10n;
        exponent++;
    }
    while (mantissa < minMantissa)
    {
        mantissa *= 10n;
        exponent--;
    }

    // canonical zero on mantissa underflow
    if (mantissa == 0)
        return 0n;

    // under and overflows
    if (exponent > maxExponent || exponent < minExponent)
        return -1; // note this is an "invalid" XFL used to propagate errors

    exponent += 97n;

    let xfl = (is_negative ? 1n : 0n);
    xfl <<= 8n;
    xfl |= BigInt(exponent);
    xfl <<= 54n;
    xfl |= BigInt(mantissa);

    return xfl;
}

function get_exponent(xfl)
{
    if (xfl < 0n)
        throw "Invalid XFL";
    if (xfl == 0n)
        return 0n;
    return ((xfl >> 54n) & 0xFFn) - 97n;
}

function get_mantissa(xfl)
{
    if (xfl < 0n)
        throw "Invalid XFL";
    if (xfl == 0n)
        return 0n;
    return xfl - ((xfl >> 54n)<< 54n);
}

function is_negative(xfl)
{
    if (xfl < 0n)
        throw "Invalid XFL";
    if (xfl == 0n)
        return false;
    return ((xfl >> 62n) & 1n) == 0n;
}


function to_string_dec(xfl)
{
    if (typeof(xfl) == 'string')
        xfl = BigInt('0x'+xfl)
    if (xfl < 0n)
        throw "Invalid XFL";
    if (xfl == 0n)
        return "<zero>";

    m = get_mantissa(xfl);
    e = parseInt(get_exponent(xfl) + '');
    ml = (m + '').length;

    p = ml + e;

    neg = is_negative(xfl) ? '-' : '';
    m = '' + m;
    if (p < 0)
        return neg + '0.' + '0'.repeat(-p) + m;
    else if (p > ml)
        return neg + m + '0'.repeat(p-ml)
    else
    {

        return neg + (p == 0 ? '0' : m.slice(0, p)) + '.' + m.slice(p)
    }
}

function to_string(xfl)
{
    if (typeof(xfl) == 'string')
        xfl = BigInt('0x'+xfl)
    if (xfl < 0n)
        throw "Invalid XFL";
    if (xfl == 0n)
        return "<zero>";
    return (is_negative(xfl) ? "-" : "") +
            get_mantissa(xfl) + " * 10^(" + get_exponent(xfl) + ")";

}

exports.xls17StringDec = to_string_dec;

