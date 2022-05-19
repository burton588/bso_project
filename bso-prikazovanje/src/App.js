import React from "react";
import './App.css';
class App extends React.Component {

	// Constructor
	constructor(props) {
		super(props);

		this.state = {
			items: [],
			DataisLoaded: false
		};
	}

	// ComponentDidMount is used to
	// execute the code
	componentDidMount() {
		fetch(
"https://j62i4034p8.execute-api.us-east-1.amazonaws.com/bsostage")
			.then((res) => res.json())
			.then((json) => {
				this.setState({
					items: json,
					DataisLoaded: true
				});
			})
	}
	render() {
    const { DataisLoaded, items } = this.state;
    if (typeof items !== "undefined") {
      var body = items["body"];
      if (typeof body !== "undefined") {
        var obj = JSON.stringify(body);
        var obj2 = JSON.parse(obj)
        var obj3 = JSON.parse(obj2)
        var itms = obj3["Items"]
        //console.log(itms);
        //var listItems = itms.map((d) => <li key={d.tmp}>{d.tmp}°C</li>);
      }
    }
        if (!DataisLoaded) return <div>
			<h1> Prosimo počakajte .... </h1> </div> ;

		return (
		<div className = "App">
			<h1> Temperatura iz senzorja </h1>
      <div className="temperatura">
        <div class="temperatura-container">
      {
      // listItems
        itms[0]["tmp"]
      }
      °C
        </div>
      </div>
		</div>
	);
}
}

export default App;
