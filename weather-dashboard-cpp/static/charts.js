// ── ECharts option builders ───────────────────────────────────────────────────

function buildLineOption(pts, color, unit) {
    return {
        tooltip: { trigger: 'axis', axisPointer: { type: 'cross' } },
        grid: { left: '8%', right: '2%', bottom: '3%', top: '8%', containLabel: true },
        xAxis: {
            type: 'time',
            axisLabel: { color: '#888', fontSize: 10, hideOverlap: true }
        },
        yAxis: {
            type: 'value',
            name: unit,
            nameTextStyle: { color: '#888', fontSize: 10 },
            axisLabel: { color: '#888', fontSize: 10 }
        },
        series: [{
            type: 'line',
            smooth: true,
            showSymbol: false,
            lineStyle: { color: color, width: 2 },
            areaStyle: {
                color: {
                    type: 'linear', x: 0, y: 0, x2: 0, y2: 1,
                    colorStops: [
                        { offset: 0, color: color + '55' },
                        { offset: 1, color: color + '00' }
                    ]
                }
            },
            data: pts
        }]
    };
}

function buildWindOption(speedData, dirData) {
    // Build direction lookup by timestamp
    const dirMap = {};
    dirData.forEach(([ts, deg]) => { dirMap[ts] = deg; });

    // Arrow: wide arrowhead, tip at top. symbolRotate = dir points in the FROM-direction.
    const arrowPts = speedData
        .filter(([ts]) => dirMap[ts] != null)
        .map(([ts]) => ({ value: [ts, 0.92], symbolRotate: dirMap[ts] }));

    const color = '#4fc3f7';
    return {
        tooltip: { trigger: 'axis' },
        grid: { left: '8%', right: '2%', bottom: '3%', top: '14%', containLabel: true },
        xAxis: {
            type: 'time',
            axisLabel: { color: '#888', fontSize: 10, hideOverlap: true }
        },
        yAxis: [
            {
                type: 'value', name: 'm/s', min: 0,
                nameTextStyle: { color: '#888', fontSize: 10 },
                axisLabel: { color: '#888', fontSize: 10 }
            },
            { type: 'value', min: 0, max: 1, show: false }
        ],
        series: [
            {
                type: 'line',
                yAxisIndex: 0,
                smooth: true,
                showSymbol: false,
                lineStyle: { color: color, width: 2 },
                areaStyle: {
                    color: {
                        type: 'linear', x: 0, y: 0, x2: 0, y2: 1,
                        colorStops: [
                            { offset: 0, color: color + '55' },
                            { offset: 1, color: color + '00' }
                        ]
                    }
                },
                data: speedData
            },
            {
                type: 'scatter',
                yAxisIndex: 1,
                silent: true,
                symbol: 'path://M 0,-10 L 5,2 L 1,2 L 1,10 L -1,10 L -1,2 L -5,2 Z',
                symbolSize: 14,
                itemStyle: { color: '#ffb300', opacity: 0.9 },
                tooltip: { show: false },
                data: arrowPts
            }
        ]
    };
}

// ── Chart initialisation ──────────────────────────────────────────────────────

function initCharts(root) {
    const dark = window.matchMedia('(prefers-color-scheme: dark)').matches;

    (root || document).querySelectorAll('.mini-chart[data-chart]').forEach(el => {
        const instance = echarts.getInstanceByDom(el)
            || echarts.init(el, dark ? 'dark' : null, { renderer: 'canvas' });

        const type = el.dataset.chart;

        if (type === 'line') {
            const pts = JSON.parse(el.dataset.points || '[]');
            if (pts.length > 0)
                instance.setOption(buildLineOption(pts, el.dataset.color, el.dataset.unit), true);

        } else if (type === 'wind') {
            const speed = JSON.parse(el.dataset.speed     || '[]');
            const dir   = JSON.parse(el.dataset.direction || '[]');
            if (speed.length > 0)
                instance.setOption(buildWindOption(speed, dir), true);
        }
    });
}

document.addEventListener('DOMContentLoaded', () => initCharts());
document.addEventListener('htmx:afterSwap',   e  => initCharts(e.detail.target));
