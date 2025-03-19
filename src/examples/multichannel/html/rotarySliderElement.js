class RotarySlider extends HTMLElement {
  constructor() {
    super();
    this.attachShadow({ mode: 'open' });

    // Default properties
    this.props = {
      bounds: { top: 10, left: 10, width: 86, height: 86 },
      range: { min: 0, max: 1, defaultValue: 0, skew: 1, increment: 0.001 },
      value: 0,
      trackerWidth: 22,
      colour: {
        fill: "#00ABD1",
        stroke: { colour: "#2d2d2d", width: 8 },
        tracker: { fill: "#77C1A4", background: "#525252" },
      },
      opacity: 1,
    };

    // Bind event listeners
    this.pointerDown = this.pointerDown.bind(this);
    this.pointerMove = this.pointerMove.bind(this);
    this.pointerUp = this.pointerUp.bind(this);

    // Render the slider
    this.render();
  }

  connectedCallback() {
    this.shadowRoot.addEventListener("pointerdown", this.pointerDown);
  }

  disconnectedCallback() {
    this.shadowRoot.removeEventListener("pointerdown", this.pointerDown);
    window.removeEventListener("pointermove", this.pointerMove);
    window.removeEventListener("pointerup", this.pointerUp);
  }

  // Handle pointer down event
  pointerDown(event) {
    this.startY = event.clientY;
    this.startValue = this.props.value;
    window.addEventListener("pointermove", this.pointerMove);
    window.addEventListener("pointerup", this.pointerUp);
  }

  // Handle pointer move event
  pointerMove(event) {
    const steps = 200;
    const valueDiff = ((this.props.range.max - this.props.range.min) * (event.clientY - this.startY)) / steps;
    const newValue = this.startValue - valueDiff;
    this.props.value = Math.min(Math.max(newValue, this.props.range.min), this.props.range.max);
    this.render();

    // Emit a custom event with the new value
    this.dispatchEvent(new CustomEvent('value-change', {
      detail: { value: this.props.value }
    }));
  }

  // Handle pointer up event
  pointerUp() {
    window.removeEventListener("pointermove", this.pointerMove);
    window.removeEventListener("pointerup", this.pointerUp);
  }

  // Render the slider
  render() {
    const { bounds, range, value, trackerWidth, colour, opacity } = this.props;
  
    // Calculate the angle for the arc
    const normalizedValue = (value - range.min) / (range.max - range.min);
    const angle = -130 + (260 * normalizedValue);
  
    // Generate SVG paths
    const describeArc = (x, y, radius, startAngle, endAngle) => {
      const start = this.polarToCartesian(x, y, radius, endAngle);
      const end = this.polarToCartesian(x, y, radius, startAngle);
      const largeArcFlag = endAngle - startAngle <= 180 ? "0" : "1";
      return `M ${start.x} ${start.y} A ${radius} ${radius} 0 ${largeArcFlag} 0 ${end.x} ${end.y}`;
    };
  
    const outerTrackerPath = describeArc(bounds.width / 2, bounds.height / 2, (bounds.width / 2) * 0.75, -130, 130);
    const trackerPath = describeArc(bounds.width / 2, bounds.height / 2, (bounds.width / 2) * 0.75, -130, angle);
  
    // Render the SVG
    this.shadowRoot.innerHTML = `
      <style>
        :host {
          display: inline-block;
          width: ${bounds.width}px;
          height: ${bounds.height}px;
        }
        svg {
          width: 100%;
          height: 100%;
        }
      </style>
      <svg viewBox="0 0 ${bounds.width} ${bounds.height}">
        <!-- Background arc (unfilled portion) -->
        <path d="${outerTrackerPath}" fill="none" stroke="${colour.tracker.background}" stroke-width="${trackerWidth}" />
        <!-- Filled arc -->
        <path d="${trackerPath}" fill="none" stroke="${colour.tracker.fill}" stroke-width="${trackerWidth}" />
        <!-- Center circle -->
        <circle cx="${bounds.width / 2}" cy="${bounds.height / 2}" r="${(bounds.width / 2) * 0.75 - trackerWidth / 2}" 
                fill="${colour.fill}" stroke="${colour.stroke.colour}" stroke-width="${colour.stroke.width}" />
      </svg>
    `;
  }

  // Helper function to convert polar to Cartesian coordinates
  polarToCartesian(centerX, centerY, radius, angleInDegrees) {
    const angleInRadians = ((angleInDegrees - 90) * Math.PI) / 180.0;
    return {
      x: centerX + radius * Math.cos(angleInRadians),
      y: centerY + radius * Math.sin(angleInRadians),
    };
  }

  // Getters and setters for attributes
  static get observedAttributes() {
    return ['min', 'max', 'value'];
  }

  attributeChangedCallback(name, oldValue, newValue) {
    if (name === 'min') {
      this.props.range.min = parseFloat(newValue);
    } else if (name === 'max') {
      this.props.range.max = parseFloat(newValue);
    } else if (name === 'value') {
      this.props.value = parseFloat(newValue);
    }
    this.render();
  }

  get value() {
    return this.props.value;
  }

  set value(newValue) {
    this.props.value = newValue;
    this.render();
  }
}

// Define the custom element
customElements.define('rotary-slider', RotarySlider);