#[derive(Debug)]
enum usState {
    Alabama,
    Alaska,
}
enum Coin{
    Pretty,
    Nickel,
    Dime,
    Quarter(usState),
}
fn value_in_cents(coin:Coin)->u8{
    match coin{
        Coin::Pretty=>1,
        Coin::Nickel=>5,
        Coin::Dime=>10,
        Coin::Quarter(state)=>{
            println!("State quater from {:?}!",state);
            25
        },
    }
}
fn main(){
    let c=Coin::Quarter(usState::Alaska);
    println!("{}",value_in_cents(c));
}